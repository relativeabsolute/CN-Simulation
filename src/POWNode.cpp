/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"
#include <numeric>

POWNode::POWNode() : messageGen(nullptr) {
}

POWNode::~POWNode() {
}

void POWNode::addNodeToGateMapping(int nodeIndex, cGate *gate) {
    EV << "Mapping node " << nodeIndex << " to gate " << gate << std::endl;
    nodeIndexToGateMap[nodeIndex] = gate;

    // also setup data for node
    peers.insert(std::make_pair(nodeIndex, std::make_unique<POWNodeData>()));
    peersProcess.push(nodeIndex);
}

void POWNode::initConnections() {
    EV << "Initializing POWNode." << std::endl;
    const char *defaultNodesStr = par("defaultNodeList").stringValue();
    std::vector<int> defaultNodes = cStringTokenizer(defaultNodesStr).asIntVector();
    const size_t pathStrSize = 6; // node + [ + ]
    char *path = new char[defaultNodes.size() + pathStrSize];
    for (auto it = defaultNodes.begin(); it != defaultNodes.end(); ++it) {
        // don't try to connect to ourselves and don't try to connect if we are offline
        if (*it != getIndex() && isOnline()) {
            EV << "Attempting to connect from " << getIndex() << " to " << *it << std::endl;
            sprintf(path, "node[%d]", *it);
            POWNode *toCheck = check_and_cast<POWNode*>(getModuleByPath(path));
            if (!toCheck->isOnline()) {
                EV << "Node " << *it << " is not online.  Moving onto next node." << std::endl;
            } else {
                connectTo(*it, toCheck);
            }
        }
    }
    delete[] path;
}

void POWNode::connectTo(int otherIndex, POWNode *other) {
    // TODO: put limit on number of gates/connections?
    cGate *destGateIn, *destGateOut;
    other->getOrCreateFirstUnconnectedGatePair("gate", false, true, destGateIn, destGateOut);

    cGate *srcGateIn, *srcGateOut;
    getOrCreateFirstUnconnectedGatePair("gate", false, true, srcGateIn, srcGateOut);

    // TODO: may want to store the connections returned by connectTo here
    srcGateOut->connectTo(destGateIn);
    EV << "Outbound connection established" << std::endl;
    addNodeToGateMapping(otherIndex, srcGateOut);
    destGateOut->connectTo(srcGateIn);
    EV << "Inbound connection established" << std::endl;
    other->addNodeToGateMapping(getIndex(), destGateOut);

    // schedule address advertisement to the peer
    scheduleAddrAd(otherIndex);
}

void POWNode::setupMessageHandlers() {
    selfMessageHandlers[MessageGenerator::MESSAGE_CHECK_QUEUES] = &POWNode::messageHandler;
    selfMessageHandlers[MessageGenerator::MESSAGE_ADVERTISE_ADDRESSES] = &POWNode::advertiseAddresses;

    messageHandlers[MessageGenerator::MESSAGE_NODE_VERSION_COMMAND] = &POWNode::handleNodeVersionMessage;
    messageHandlers[MessageGenerator::MESSAGE_VERACK_COMMAND] = &POWNode::handleVerackMessage;
    messageHandlers[MessageGenerator::MESSAGE_REJECT_COMMAND] = &POWNode::handleRejectMessage;
    messageHandlers[MessageGenerator::MESSAGE_GETADDR_COMMAND] = &POWNode::handleGetAddrMessage;
}

void POWNode::internalInitialize() {
    // internal set up setup 0:
    // read constant NED parameters
    // this is mostly for convenience
    readConstantParameters();

    // internal set up step 1:
    // connect message handlers
    setupMessageHandlers();

    // internal set up step 2:
    // read peer "addresses" from this node's peers.dat
    // NOTE: this does NOT set up connections to these peers
    addrMan = std::make_unique<AddrManager>(0.5);
}

void POWNode::readConstantParameters() {
    versionNumber = par("version").intValue();
    minAcceptedVersionNumber = par("minAcceptedVersion").intValue();
    threadScheduleInterval = par("threadScheduleInterval").intValue();
    maxMessageProcess = par("maxMessageProcess").intValue();
    maxAddrAd = par("maxAddrAd").intValue();

    messageGen = std::make_unique<MessageGenerator>(versionNumber);
}

void POWNode::initialize() {
    internalInitialize();

    // set up step 1:
    // attempt to establish connections with the list of default nodes
    // TODO: have node check for its data file, and if there is one read list of known nodes from it
    initConnections();

    // step 2:
    // set up thread simulation schedule
    // just need to schedule one self message because the handler will schedule the next one
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_CHECK_QUEUES, ""));

    // step 3:
    // send node version message on outbound connections
    auto msg = messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_NODE_VERSION_COMMAND, "");

    // TODO: schedule messages so that simulation occurs in version, verack, version, etc. instead
    // of all versions then all veracks
    broadcastMessage(msg);
}

void POWNode::broadcastMessage(POWMessage *msg) {
    EV << "Broadcasting " << msg << std::endl;
    for (auto mapIterator = nodeIndexToGateMap.begin(); mapIterator != nodeIndexToGateMap.end(); ++mapIterator) {
        // it->first is the index of the destination node
        // it->second is the gate index to send over
        cMessage *copy = msg->dup();
        send(copy, mapIterator->second);
    }
    delete msg;
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
    // TODO: BTC checks a received data buffer for this index and calls processGetData if it's not empty
    auto peer = peers.find(peerIndex);
    bool moreWork = false;
    if (peer != peers.end()) {
        if (peer->second->flags.test(Disconnect)) {
            EV << "Node " << peerIndex << " scheduled for disconnect.  Not processing incoming message." << std::endl;
            return false;
        }
        // TODO: BTC checks the received data buffer again here, and returns true if it is not empty
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

        // TODO: BTC checks message checksum here
        processMessage(msg);

        // TODO: BTC checks received data buffer again here

        // TODO: BTC sends reject messages and checks for banned peers here
    } else {
        EV << "Attempted to process messages for nonexistant node " << peerIndex << std::endl;
    }
    return moreWork;
}

void POWNode::scheduleAddrAd(int peerIndex) {
    std::string data = "peerIndex=" + std::to_string(peerIndex);
    // same interval as threadSchedule since BTC does this task as part of that thread
    scheduleAt(simTime() + poisson((double)(int64_t)threadScheduleInterval), messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADVERTISE_ADDRESSES, data));
}

void POWNode::advertiseAddresses(POWMessage *msg) {
    std::string data = msg->getData();
    // data is form peerIndex=<index>
    // so just get the substring after the equal sign
    int adTarget = std::stoi(data.substr(data.find('=') + 1));
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
                        sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADDR_COMMAND, data), adTarget);
                        addresses.clear();
                    }
                }
            }
            peer->second->addressesToBeSent.clear();
            if (!addresses.empty()) {
                std::string data = "addresses=";
                data += std::accumulate(addresses.begin() + 1, addresses.end(), std::to_string(addresses[0]),
                        [](const std::string &a, int b) { return a + "," + std::to_string(b); });
                sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADDR_COMMAND, data), adTarget);
            }
        } else {
            EV << "Cannot advertise to nonexistant peer " << adTarget << std::endl;
        }
    }

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
        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_REJECT_COMMAND, "reason=obsolete,disconnect=true"), sourceNode);
        disconnectNode(sourceNode);
    } else {
        // TODO: BTC stores starting height of incoming node
        EV << "Node " << sourceNode << " is compatible.  Sending VERACK." << std::endl;
        peers[sourceNode]->version = sourceVersionNo;

        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_VERACK_COMMAND, ""), sourceNode);
        // TODO: advertise our address if the connection is outbound
        // TODO: BTC does a check for listen flag and not isInitialBlockDownload
        peers[sourceNode]->addressesToBeSent.insert(meNode);

        // TODO: BTC defines an ideal number of addresses
        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_GETADDR_COMMAND, ""), sourceNode);
        peers[sourceNode]->flags.set(HasGetAddr);
         // TODO: BTC marks the address as good
    }
}

void POWNode::handleVerackMessage(POWMessage *msg) {
    peers[msg->getSource()]->flags.set(SuccessfullyConnected);
}

void POWNode::handleRejectMessage(POWMessage *msg) {
    std::string msgData = msg->getData();
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

void POWNode::handleGetAddrMessage(POWMessage *msg) {
    int messageSource = msg->getSource();
    if (peers[messageSource]->flags.test(HasSentAddr)) {
        return;
    }
    peers[messageSource]->flags.set(HasSentAddr);
    auto pushAddresses = addrMan->getRandomAddresses();
    EV << "Adding " << pushAddresses.size() << " addresses to be sent to node " << messageSource << std::endl;
    for (auto it = pushAddresses.begin(); it != pushAddresses.end(); ++it) {
        peers[messageSource]->addressesToBeSent.insert(*it);
    }
}

std::map<std::string, std::string> POWNode::dataToMap(const std::string &messageData) const {
    cStringTokenizer tokenizer(messageData.c_str(), ",");
    std::map<std::string, std::string> result;
    while (tokenizer.hasMoreTokens()) {
        cStringTokenizer miniToken(tokenizer.nextToken(), "=");
        result.insert(std::make_pair<std::string,std::string>(miniToken.nextToken(), miniToken.nextToken()));
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
