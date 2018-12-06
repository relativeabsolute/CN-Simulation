/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"
#include <numeric>
#include <set>
#include <fstream>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

POWNode::POWNode() : messageGen(nullptr) {
}

POWNode::~POWNode() {
    // dump any outgoing or incoming messages
    for (auto &kv : peers) {
        kv.second->incomingMessages.clear();
    }
}

#if(1) // initialization steps
void POWNode::addNodeToGateMapping(int nodeIndex, cGate *gate, bool inboundValue) {
    EV << "Mapping node " << nodeIndex << " to gate " << gate << std::endl;
    nodeIndexToGateMap[nodeIndex] = gate;

    // also setup data for node
    peers.insert(std::make_pair(nodeIndex, std::make_unique<POWNodeData>()));
    peers[nodeIndex]->flags.set(Inbound, inboundValue);
    peersProcess.push(nodeIndex);
}

void POWNode::initConnections() {
    EV << "Initializing POWNode." << std::endl;

    int meIndex = getIndex();
    // only connect if we are online and we are not a default node
    if (isOnline() && std::find(defaultNodes.begin(), defaultNodes.end(), meIndex) == defaultNodes.end()) {
        for (int addr : addrMan->allAddresses()) {
            if (addr != meIndex) {
                EV << "Attempting to connect from " << meIndex << " to " << addr << std::endl;
                //sprintf(path, "node[%d]", addr);
                POWNode *toCheck = getPeerNodeByPath(addr);
                if (!toCheck->isOnline()) {
                    EV << "Node " << addr << " is not online.  Moving onto next node." << std::endl;
                } else {
                    connectTo(addr, toCheck);
                }
            }
        }
    }
    //delete[] path;
}

void POWNode::connectTo(int otherIndex, POWNode *other) {
    int meIndex = getIndex();
    if (otherIndex == meIndex) {
        // don't connect to ourselves
        return;
    }
    // TODO: put limit on number of gates/connections?
    cGate *destGateIn, *destGateOut;
    other->getOrCreateFirstUnconnectedGatePair("gate", false, true, destGateIn, destGateOut);

    cGate *srcGateIn, *srcGateOut;
    getOrCreateFirstUnconnectedGatePair("gate", false, true, srcGateIn, srcGateOut);

    // TODO: may want to store the connections returned by connectTo here
    srcGateOut->connectTo(destGateIn);
    EV << meIndex << " to " << otherIndex << " connection type: outbound" << std::endl;
    addNodeToGateMapping(otherIndex, srcGateOut, false); // we are initiating
    destGateOut->connectTo(srcGateIn);
    EV << otherIndex << " to " << meIndex << " connection type: inbound" << std::endl;
    other->addNodeToGateMapping(meIndex, destGateOut, true);

    std::string bubbleMessage = "Connection established with peer " + std::to_string(otherIndex);
    bubble(bubbleMessage.c_str());

    // BTC schedules proactive address advertisements, but we are going to use a polling approach
    // scheduleAddrAd(otherIndex);
}

void POWNode::setupMessageHandlers() {
    selfMessageHandlers[MessageGenerator::MESSAGE_CHECK_QUEUES] = &POWNode::messageHandler;
    selfMessageHandlers[MessageGenerator::MESSAGE_ADVERTISE_ADDRESSES] = &POWNode::advertiseAddresses;
    selfMessageHandlers[MessageGenerator::MESSAGE_DUMP_ADDRS] = &POWNode::dumpAddresses;
    selfMessageHandlers[MessageGenerator::MESSAGE_POLL_ADDRS] = &POWNode::pollAddresses;

    messageHandlers[MessageGenerator::MESSAGE_NODE_VERSION_COMMAND] = &POWNode::handleNodeVersionMessage;
    messageHandlers[MessageGenerator::MESSAGE_VERACK_COMMAND] = &POWNode::handleVerackMessage;
    messageHandlers[MessageGenerator::MESSAGE_REJECT_COMMAND] = &POWNode::handleRejectMessage;
    messageHandlers[MessageGenerator::MESSAGE_GETADDR_COMMAND] = &POWNode::handleGetAddrMessage;
    messageHandlers[MessageGenerator::MESSAGE_ADDRS_COMMAND] = &POWNode::handleAddrsMessage;
}

void POWNode::internalInitialize() {
    // internal set up setup 0:
    // read constant NED parameters
    // this is mostly for convenience
    readConstantParameters();

    // internal set up step 1:
    // connect message handlers
    setupMessageHandlers();

    // internal set up step 2a:
    // create data directory if necessary
    EV << "Current path: " << fs::current_path().string() << std::endl;
    fs::create_directory(dataDir);
    // step 2b: read peer "addresses" from this node's peers.dat
    // NOTE: this does NOT set up connections to these peers
    addrMan = std::make_unique<AddrManager>(randomAddressFraction);
    readAddresses();
}

void POWNode::readAddresses() {
    std::vector<int> addresses;
    if (newNetwork || !fs::exists(addressesFile)) {
        EV << "Addresses file " << addressesFile << " for node " << getIndex() << " does not exist.  Reading default nodes." << std::endl;
        addresses.assign(defaultNodes.begin(), defaultNodes.end());
    } else {
        EV << "Reading known peer addresses for node " << getIndex() << std::endl;
        std::ifstream fileReader(addressesFile, std::ios::in | std::ios::binary);
        if (fileReader) {
            std::string fileContents;
            fileReader >> fileContents;
            addresses = stringAsVector(fileContents);
        }
    }
    EV << "Addresses: ";
    for (int addr : addresses) {
        EV << addr << " ";
    }
    EV << std::endl;
    addrMan->addAddresses(addresses);
}

void POWNode::readConstantParameters() {
    versionNumber = par("version").intValue();
    minAcceptedVersionNumber = par("minAcceptedVersion").intValue();
    threadScheduleInterval = par("threadScheduleInterval").intValue();
    maxMessageProcess = par("maxMessageProcess").intValue();
    maxAddrAd = par("maxAddrAd").intValue();
    numAddrRelay = par("numAddrRelay").intValue();
    addrRelayVecSize = par("addrRelayVecSize").intValue();
    dumpAddressesInterval = par("dumpAddressesInterval").intValue();
    dataDir = par("dataDir").stringValue();
    addressesFile = (fs::path(dataDir) / ("peers" + std::to_string(getIndex()) + ".txt")).string();
    const char *defaultNodesStr = par("defaultNodeList").stringValue();
    defaultNodes = cStringTokenizer(defaultNodesStr).asIntVector();
    randomAddressFraction = par("randomAddressFraction").doubleValue();
    newNetwork = par("newNetwork").boolValue();

    messageGen = std::make_unique<MessageGenerator>(versionNumber);
}

void POWNode::scheduleSelfMessages() {
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_CHECK_QUEUES, ""));
    scheduleAt(simTime() + dumpAddressesInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_DUMP_ADDRS, ""));

    // initial address poll delayed to allow initial connections to be built up
    scheduleAt(simTime() + 2 * threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_POLL_ADDRS, ""));
}

void POWNode::initialize() {
    internalInitialize();

    // set up step 1:
    // attempt to establish connections with the list of default nodes
    // TODO: have node check for its data file, and if there is one read list of known nodes from it
    initConnections();

    // step 2:
    // set up self scheduled messages
    // just need to schedule one self message because the handler will schedule the next one
    scheduleSelfMessages();

    // step 3:
    // send node version message on outbound connections
    auto msg = messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_NODE_VERSION_COMMAND, "");

    EV << "Broadcasting node version message to outbound peers." << std::endl;
    broadcastMessage(msg, [&, this](int peer){ return !this->peers[peer]->flags.test(Inbound); });
}

void POWNode::broadcastMessage(POWMessage *msg, std::function<bool(int)> predicate) {
    EV << "Broadcasting " << msg << std::endl;
    int successCounter = 0;
    for (auto mapIterator = nodeIndexToGateMap.begin(); mapIterator != nodeIndexToGateMap.end(); ++mapIterator) {
        // it->first is the index of the destination node
        // it->second is the gate index to send over
        if (predicate(mapIterator->first)) {
            ++successCounter;
            cMessage *copy = msg->dup();
            send(copy, mapIterator->second);
        }
    }
    delete msg;
    EV << "Message broadcasted to " << successCounter << " of " << nodeIndexToGateMap.size() << " peers." << std::endl;
}
#endif

#if(1) // handle incoming self messages
void POWNode::sendOutgoingMessages(int peerIndex) {
    auto peer = peers.find(peerIndex);
    if (peer != peers.end()) {
        if (!peer->second->flags.test(SuccessfullyConnected) || peer->second->flags.test(Disconnect)) {
            EV << "No connection with node " << peerIndex << ".  Not sending outgoing data." << std::endl;
            return;
        }
        // Note: we don't handle address advertisement here since that's done on a poisson distributed
        // time interval, so it can just be a scheduled message
        // TODO: block sync
        // TODO: send wallet transactions not in a block yet
        // TODO: do block announcements via headers
        // TODO: inventory
        // TODO: detect if peer is stalling
        // TODO: check number of blocks in flight
        // TODO: check for headers sync timeouts
        // TODO: check outbound peers have reasnable chains
        // TODO: getdata blocks
        // TODO: getdata non blocks
    } else {
        EV << "Attempted to send data to nonexistant node " << peerIndex << std::endl;
    }
}

void POWNode::processMessage(POWMessage *msg) {
    std::string methodName = msg->getName();
    auto mapIt = messageHandlers.find(methodName);
    if (mapIt != messageHandlers.end()) {
        EV << "Processing message " << msg << std::endl;
        if (checkMessageInScope(msg)) {
            EV <<  "Message in scope." << std::endl;
            mapIt->second(*this, msg);
        } else {
            EV << "Message not in scope." << std::endl;
            // TODO: mark misbehaving
        }
    } else {
        EV << "Handler not found for message of type " << methodName << std::endl;
    }
    delete msg;
}

bool POWNode::processIncomingMessages(int peerIndex) {
    // TODO: check a received data buffer for this index and calls processGetData if it's not empty
    auto peer = peers.find(peerIndex);
    bool moreWork = false;
    if (peer != peers.end()) {
        if (peer->second->flags.test(Disconnect)) {
            EV << "Node " << peerIndex << " scheduled for disconnect.  Not processing incoming message." << std::endl;
            return false;
        }
        // TODO: check the received data buffer again here, and return true if it is not empty
        if (peer->second->flags.test(PauseSend)) {
            EV << "Send buffer for node " << peerIndex << " is full.  Not processing incoming message." << std::endl;
            return false;
        }

        if (peer->second->incomingMessages.empty()) {
            EV << "No messages to process for node " << peerIndex << std::endl;
            return false;
        }
        POWMessage *msg = peer->second->incomingMessages.front();
        peer->second->incomingMessages.pop_front();
        peer->second->flags[PauseReceive] = 0; // TODO: set if the queue size is greater than receiveFloodSize
        moreWork = !peer->second->incomingMessages.empty();

        // TODO: check checksum here
        processMessage(msg);

        // TODO: check received data buffer (again) here

        // TODO: send reject messages and check for banned peers
    } else {
        EV << "Attempted to process messages for nonexistant node " << peerIndex << std::endl;
    }
    return moreWork;
}

void POWNode::pollAddresses(POWMessage *msg) {
    int meIndex = getIndex();
    EV << "Polling successfully connected peers for connections." << std::endl;
    broadcastMessage(messageGen->generateMessage(meIndex, MessageGenerator::MESSAGE_GETADDR_COMMAND, ""),
            [&, this](int peer){ return this->peers[peer]->flags.test(SuccessfullyConnected); });
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(meIndex, MessageGenerator::MESSAGE_POLL_ADDRS, ""));
}

void POWNode::scheduleAddrAd(int peerIndex) {
    std::string data = "peerIndex=" + std::to_string(peerIndex);
    // same interval as threadSchedule since BTC does this task as part of that thread
    scheduleAt(simTime() + poisson((double)(int64_t)threadScheduleInterval), messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADVERTISE_ADDRESSES, data));
}

void POWNode::advertiseAddresses(POWMessage *msg) {
    /*
    // data is form peerIndex=<index>
    EV << "Performing scheduled address advertisement.  Message that triggered this: " << msg->getFullName() << std::endl;
    EV_DETAIL << "Message data: " << msg->getData() << std::endl;
    std::map<std::string, std::string> messageData = dataToMap(msg->getData());
    std::string peerIndexStr = messageData["peerIndex"];
    EV_DETAIL << "Target peer: \"" << peerIndexStr << "\"" << std::endl;
    int adTarget = std::stoi(messageData["peerIndex"]);
    if (!peers[adTarget]->flags.test(SuccessfullyConnected) || peers[adTarget]->flags.test(Disconnect)) {
        EV << "Peer " << adTarget << " disconnected.  Not advertising addresses." << std::endl;
    } else {
        scheduleAddrAd(adTarget);
        auto peer = peers.find(adTarget);
        if (peer != peers.end()) {
            std::vector<int> addresses(peer->second->addressesToBeSent.size());
            for (int address : peer->second->addressesToBeSent) {
                if (peer->second->knownAddresses.find(address) != peer->second->knownAddresses.end()) {
                    peer->second->knownAddresses.insert(address);
                    addresses.push_back(address);
                    if (addresses.size() >= maxAddrAd) {
                        std::string data = "addresses=";
                        data += std::accumulate(addresses.begin() + 1, addresses.end(), std::to_string(addresses[0]),
                                [](const std::string &a, int b) { return a + "," + std::to_string(b); });
                        EV << "Advertising " << addresses.size() << " to peer " << adTarget << std::endl;
                        EV_DETAIL << "Advertisement contents: " << vectorAsString(addresses) << std::endl;
                        sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADDR_COMMAND, data), adTarget);
                        addresses.clear();
                    }
                }
            }
            peer->second->addressesToBeSent.clear();
            if (!addresses.empty()) {
                EV << "Advertising " << addresses.size() << " to peer " << adTarget << std::endl;
                EV_DETAIL << "Advertisement contents: " << vectorAsString(addresses) << std::endl;
                std::string data = "addresses=";
                data += std::accumulate(addresses.begin() + 1, addresses.end(), std::to_string(addresses[0]),
                        [](const std::string &a, int b) { return a + "," + std::to_string(b); });
                sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADDR_COMMAND, data), adTarget);
            }
        } else {
            EV << "Cannot advertise to nonexistant peer " << adTarget << std::endl;
        }
    }
    scheduleAddrAd(adTarget);
    */
}

void POWNode::messageHandler(POWMessage *msg) {
    // only process a certain number of messages at once
    // need to ensure that each node gets a fair chance at being processed
    EV << "Handling messages in node " << getIndex() << std::endl;
    for (currentMessagesProcessed = 0; currentMessagesProcessed < maxMessageProcess; ++currentMessagesProcessed) {
        int peerIndex = peersProcess.front();
        peersProcess.pop();
        EV << "Processing and sending messages for node " << peerIndex << std::endl;
        auto peerIt = peers.find(peerIndex);
        if (!peerIt->second->flags.test(Disconnect)) {
            processIncomingMessages(peerIndex);
            sendOutgoingMessages(peerIndex);
            peersProcess.push(peerIndex);
        } // don't put the peer back on if it's disconnected
    }
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_CHECK_QUEUES, ""));
}

void POWNode::handleSelfMessage(POWMessage *msg) {
    std::string methodName = msg->getName();
    auto handlerIt = selfMessageHandlers.find(methodName);
    if (handlerIt != selfMessageHandlers.end()) {
        handlerIt->second(*this, msg);
    } else {
        EV << "No handler for self message " << msg << std::endl;
    }
}

void POWNode::dumpAddresses(POWMessage *msg) {
    // probably could optimize to just write new addresses but that's complicated
    // TODO: determine if we need to do banlist stuff
    EV << "Dumping known addresses for node " << getIndex() << std::endl;
    std::ofstream fileWriter(addressesFile, std::ios::out | std::ios::trunc);
    if (fileWriter) {
        EV << "File opened for writing successfully" << std::endl;
        auto addrs = addrMan->allAddresses();
        EV << "Writing " << addrs.size() << " to file." << std::endl;
        int i = 0;
        for (auto it = addrs.begin(); it != addrs.end(); ++it) {
            fileWriter << std::to_string(*it);
            if (i++ != addrs.size() - 1) {
                fileWriter << ",";
            }
        }
        fileWriter.flush();
        fileWriter.close();
    } else {
        EV << "Data file could not be written to." << std::endl;
    }
    scheduleAt(simTime() + dumpAddressesInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_DUMP_ADDRS, ""));
}
#endif

#if(1) // handle incoming messages from peers
void POWNode::handleMessage(cMessage *msg) {
    POWMessage *powMessage = check_and_cast<POWMessage *>(msg);
    logReceivedMessage(powMessage);
    if (powMessage->isSelfMessage()) {
        EV << "Received scheduler message.  Sending to appropriate handler." << std::endl;
        handleSelfMessage(powMessage);
        delete msg;
    } else {
        int source = powMessage->getSource();
        EV << "Adding message to queue for peer " << source << std::endl;
        EV_DETAIL << "Message data: " << powMessage->getData() << std::endl;
        peers[source]->incomingMessages.push_back(powMessage);
        // don't delete here because the message needs to be processed
    }
}

void POWNode::handleNodeVersionMessage(POWMessage *msg) {
    int sourceNode = msg->getSource();
    int meNode = getIndex();
    int sourceVersionNo = msg->getVersionNo();
    if (sourceVersionNo < minAcceptedVersionNumber) {
        // disconnect from nodes that are too old
        EV << "Node " << sourceNode << " using obsolete protocol version.  Minimum accepted version is " << minAcceptedVersionNumber << std::endl;
        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_REJECT_COMMAND, "reason=obsolete;disconnect=true"), sourceNode);
        disconnectNode(sourceNode);
    } else {
        std::string bubbleMessage = "Received valid version message from " + std::to_string(sourceNode);
        bubble(bubbleMessage.c_str());
        bool sourceInbound = peers[sourceNode]->flags.test(Inbound);
        // TODO: store starting height of incoming node
        if (sourceInbound) {
            EV << "Sending node version message to inbound peer " << sourceNode << std::endl;
            sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_NODE_VERSION_COMMAND, ""), sourceNode);
        }

        EV << "Node " << sourceNode << " is compatible.  Sending VERACK." << std::endl;
        peers[sourceNode]->version = sourceVersionNo;

        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_VERACK_COMMAND, ""), sourceNode);

        /* BTC asks for addresses upon receiving version message, but we use a polling approach
        if (!sourceInbound) {
            EV << "Adding self address " << meNode << " to addresses to be sent to outbound peer " << sourceNode << std::endl;
            // TODO: check for listen flag and not isInitialBlockDownload
            peers[sourceNode]->addressesToBeSent.insert(meNode);

            EV << "Sending addresses request on outbound connection." << std::endl;
            // TODO: check for ideal number of addresses
            sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_GETADDR_COMMAND, ""), sourceNode);
            peers[sourceNode]->flags.set(HasGetAddr);
        }
        */
         // TODO: mark the address as good
    }
}

void POWNode::handleVerackMessage(POWMessage *msg) {
    int sourceIndex = msg->getSource();
    std::string connectionType = "inbound";
    if (!peers[sourceIndex]->flags.test(Inbound))  {
        // TODO: mark the node's state with the currently connected flag, so the timestamp is updated later
        connectionType = "outbound";
    } else {
        // if this is an inbound connection it might be from a node we didn't know about before
        EV_DETAIL << "Adding peer " << sourceIndex << " to known indexes of peer " << getIndex() << std::endl;
        addrMan->addAddress(sourceIndex);
    }
    std::string bubbleMessage = "Handling verack message from " + connectionType + " peer " + std::to_string(sourceIndex);
    bubble(bubbleMessage.c_str());
    EV << "Handling verack message from " << connectionType << " peer " << sourceIndex << std::endl;
    EV << "Marking peer " << sourceIndex << " as successfully connected." << std::endl;
    peers[msg->getSource()]->flags.set(SuccessfullyConnected);
}

void POWNode::handleRejectMessage(POWMessage *msg) {
    std::string msgData = msg->getData();
    int source = msg->getSource();
    std::string bubbleMessage = "Received reject message from " + std::to_string(source) + " containing data: " + msgData;
    bubble(bubbleMessage.c_str());
    if (msgData.empty()) {
        EV << "Invalid reject message.  Message must contain data." << std::endl;
    } else {
        auto dataParamMap = dataToMap(msgData);
        EV << "Reject reason: " << dataParamMap["reason"] << std::endl;
        if (dataParamMap["disconnect"] == "true") {
            disconnectNode(msg->getSource());
        }
    }
}

void POWNode::relayAddress(int address) {
    // NOTE: BTC does some hashing nonsense to determine the best nodes to send to, but we'll just pick two random ones
    /* TODO: unsure if we should still relay addresses while polling
     * or just poll more frequently
    EV << "Relaying addresses" << std::endl;
    for (int a : addrMan->getRandomAddresses()) {
        peers[a]->addressesToBeSent.insert(address);
    }
    */
}

void POWNode::handleAddrsMessage(POWMessage *msg) {
    int messageSource = msg->getSource();
    std::string bubbleMessage = "handling addrs message from peer: " + std::to_string(messageSource) + " containing data " + msg->getData();
    bubble(bubbleMessage.c_str());
    EV << "Handling addrs message from peer " << messageSource << std::endl;
    std::vector<int> newAddresses = getMessageDataVector(msg, "addresses");
    EV << "Received " << newAddresses.size() << " addresses from node " << messageSource << std::endl;
    // TODO: attempt to connect to some if not all of the new peers
    dynamicConnect(newAddresses);
    addrMan->addAddresses(newAddresses);
}

void POWNode::handleAddrMessage(POWMessage *msg) {
    /* see handleAddrsMessage (we are using a polling approach)
    int messageSource = msg->getSource();
    std::string bubbleMessage = "handling addr message from peer " + std::to_string(messageSource) + " containing data " + msg->getData();
    bubble(bubbleMessage.c_str());
    EV << "Handling addr message from peer " << messageSource << std::endl;
    std::vector<int> newAddresses = getMessageDataVector(msg, "addresses");
    if (newAddresses.size() > maxAddrAd) {
        // TODO: mark peer as misbehaving
        return;
    }
    EV << "Received addresses: " << newAddresses.size() << " from node " << messageSource << std::endl;
    std::vector<int> okAddresses;
    for (int addr : newAddresses) {
        peers[messageSource]->knownAddresses.insert(addr);

        // NOTE: BTC checks if the address' time stamp <= 100000000 or if it is greater than 10 minutes from now

        // NOTE: BTC does a timestamp comparison, checks if the source has sent a getaddr message, checks the size of the addresses vector,
        // and checks if the address is routable
        // we'll only worry about the relay size
        if (newAddresses.size() < addrRelayVecSize) {
            relayAddress(addr);
        }
        okAddresses.push_back(addr);
    }
    if (newAddresses.size() < maxAddrAd) {
        peers[messageSource]->flags.set(HasGetAddr, false);
    }
    addrMan->addAddresses(okAddresses);
    */
}

void POWNode::handleGetAddrMessage(POWMessage *msg) {
    int messageSource = msg->getSource();
    std::string bubbleMessage = "Handling get_addr message from " + std::to_string(messageSource);
    bubble(bubbleMessage.c_str());
    EV << "Handling getaddr message from peer " << messageSource << std::endl;
    /* BTC only allows getaddr messages on inbound connections to prevent fingerprinting attacks
     * but we can ignore that herer
    if (!peers[messageSource]->flags.test(Inbound)) {
        EV << "Ignoring getaddr message from outbound connection peer " << messageSource << std::endl;
        return;
    }
    */

    /* Bitcoin ignores repeated getaddr messages, but we are using a polling approach
    if (peers[messageSource]->flags.test(HasSentAddr)) {
        EV << "Ignoring repeated getaddr message from peer " << messageSource << std::endl;
        return;
    }
    peers[messageSource]->flags.set(HasSentAddr);
    */

    /* with polling approach, we just send addresses as soon as they are requested, instead of putting them on a queue to be sent later
    peers[messageSource]->addressesToBeSent.clear();
    auto pushAddresses = addrMan->getRandomAddresses();
    EV << "Adding " << pushAddresses.size() << " addresses to be sent to node " << messageSource << std::endl;
    for (auto it = pushAddresses.begin(); it != pushAddresses.end(); ++it) {
        peers[messageSource]->addressesToBeSent.insert(*it);
    }
    */
    auto addresses = addrMan->getRandomAddresses();
    EV_DETAIL << "Sending addresses " << vectorAsString(addresses) << " to peer " << messageSource << std::endl;
    sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADDRS_COMMAND,
            "addresses=" + vectorAsString(addresses)), messageSource);
}
#endif

#if(1) // utility functions
std::vector<int> POWNode::getMessageDataVector(const POWMessage *msg, const std::string &vectorName) const {
    EV_DETAIL << "Getting message data vector for parameter \'" << vectorName << "\' from message " << msg << std::endl;
    auto dataMap = dataToMap(msg->getData());
    EV_DETAIL << "Data map assigned." << std::endl;
    return stringAsVector(dataMap[vectorName]);
}

void POWNode::dynamicConnect(const std::vector<int> &newAddresses) {
    EV << "Dynamically connecting to new addresses." << std::endl;
    EV_DETAIL << "Addresses before checking for connections: " << vectorAsString(newAddresses) << std::endl;
    std::vector<int> toAdd;
    std::remove_copy_if(newAddresses.begin(), newAddresses.end(),
            std::back_inserter(toAdd), [&, this](int peer){ return this->nodeIndexToGateMap.find(peer) != this->nodeIndexToGateMap.end(); });
    EV_DETAIL << "Addresses after checking for connections: " << vectorAsString(toAdd) << std::endl;
    // connect to half the peers to even out the number of inbound and outbound connections for each node
    int newCount = 0;
    for (int i = 0; i < toAdd.size() / 2; ++i) {
        ++newCount;
        int otherIndex = toAdd[i];
        connectTo(otherIndex, getPeerNodeByPath(otherIndex));
    }
    EV_DETAIL << "Dynamically connected to " << newCount << " of " << newAddresses.size() << " advertised peers." << std::endl;
}

std::vector<int> POWNode::stringAsVector(const std::string &vector) const {
    EV_DETAIL << "stringAsVector, source string = " << vector << std::endl;
    return cStringTokenizer(vector.c_str(), ",").asIntVector();
}

std::map<std::string, std::string> POWNode::dataToMap(const std::string &messageData) const {
    EV_DETAIL << "dataToMap, messageData=" << messageData << std::endl;
    cStringTokenizer tokenizer(messageData.c_str(), ";");
    EV_DETAIL << "tokenizer constructed." << std::endl;
    std::map<std::string, std::string> result;
    while (tokenizer.hasMoreTokens()) {
        cStringTokenizer miniToken(tokenizer.nextToken(), "=");
        EV_DETAIL << "minitokenizer constructed." << std::endl;
        if (miniToken.hasMoreTokens()) {
            std::string first = miniToken.nextToken();
            EV_DETAIL << "first token added." << std::endl;
            if (miniToken.hasMoreTokens()) {
                std::string second = miniToken.nextToken();
                EV_DETAIL << "second token added." << std::endl;
                auto pair = std::make_pair<std::string&,std::string&>(first, second);
                EV_DETAIL << "Resulting pair in map: first = \"" << pair.first << "\", second = \"" << pair.second  << "\"" << std::endl;
                result.insert(pair);
            }
        }
    }
    return result;
}

void POWNode::disconnectNode(int nodeIndex) {
    EV << "Disconnecting from node " << nodeIndex << std::endl;
    auto mapIt = nodeIndexToGateMap.find(nodeIndex);
    mapIt->second->disconnect();
    nodeIndexToGateMap.erase(mapIt);
}

void POWNode::logReceivedMessage(POWMessage *msg) const {
    EV << msg->getName() << " message received by " << getIndex() << " from " << msg->getSource() << std::endl;
}

void POWNode::sendToNode(POWMessage *msg, int nodeIndex) {
    auto mapIt = nodeIndexToGateMap.find(nodeIndex);
    if (mapIt != nodeIndexToGateMap.end()) {
        send(msg, mapIt->second);
    } else {
        EV << "Node " << nodeIndex << " not found.  Message not sent." << std::endl;
    }
}


bool POWNode::isOnline() const {
    return par("online").boolValue();
}

bool POWNode::checkMessageInScope(POWMessage *msg) {
    std::string messageName = msg->getName();
    if (messageGen->messageInScope(messageName, PreVersion)) {
        return true;
    }
    // need to check if we have a version for the incoming node
    int nodeSource = msg->getSource();
    if (peers[nodeSource]->version == 0) {
        // TODO: set misbehavior score?
        return false;
    }
    if (!messageGen->messageInScope(messageName, PreVerack)) {
        if (!peers[nodeSource]->flags[SuccessfullyConnected]) {
            // TODO: set misbehavior score
            return false;
        }
    }
    return true;
}
#endif
