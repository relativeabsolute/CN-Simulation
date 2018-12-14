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
#include <boost/algorithm/string/predicate.hpp>
#include "messages/messages.h"
#include "POWScheduler.h"

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
    if (isMiner) {
        selfMessageHandlers[MessageGenerator::MESSAGE_MINE] = &POWNode::mineHandler;
    }

    messageHandlers[MessageGenerator::MESSAGE_NODE_VERSION_COMMAND] = &POWNode::handleNodeVersionMessage;
    messageHandlers[MessageGenerator::MESSAGE_VERACK_COMMAND] = &POWNode::handleVerackMessage;
    messageHandlers[MessageGenerator::MESSAGE_REJECT_COMMAND] = &POWNode::handleRejectMessage;
    messageHandlers[MessageGenerator::MESSAGE_GETADDR_COMMAND] = &POWNode::handleGetAddrMessage;
    messageHandlers[MessageGenerator::MESSAGE_ADDRS_COMMAND] = &POWNode::handleAddrsMessage;
    messageHandlers[MessageGenerator::MESSAGE_HEADERS_COMMAND] = &POWNode::handleHeadersMessage;
    messageHandlers[MessageGenerator::MESSAGE_TX_COMMAND] = &POWNode::handleTxMessage;
    messageHandlers[MessageGenerator::MESSAGE_GETBLOCKS_COMMAND] = &POWNode::handleGetBlocksMessage;

    if (isMiner) {
        simulationScheduleHandlers[POWScheduler::SCHEDULER_MESSAGE_NEW_BLOCK] = &POWNode::handleNewBlock;
    }
    simulationScheduleHandlers[POWScheduler::SCHEDULER_MESSAGE_TX] = &POWNode::handleNewTx;
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

    // step 2c: load blockchain
    initBlockchain();
}

void POWNode::initBlockchain() {
    EV << "Loading block chain" << std::endl;
    if (newNetwork || !(blockchain = Blockchain::readFromDirectory(blocksDir))) {
        blockchain = Blockchain::emptyBlockchain(blocksPerFile);
    }
    chainHeight = blockchain->chainHeight();
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
            addresses = cStringTokenizer(fileContents.c_str(), ",").asIntVector();
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
    blocksDir = (fs::path(dataDir) / "blocks" / ("peer" + std::to_string(getIndex()))).string();
    const char *defaultNodesStr = par("defaultNodeList").stringValue();
    defaultNodes = cStringTokenizer(defaultNodesStr).asIntVector();
    randomAddressFraction = par("randomAddressFraction").doubleValue();
    newNetwork = par("newNetwork").boolValue();
    blocksPerFile = par("blocksPerFile").intValue();
    auto minersList = cStringTokenizer(par("minersList").stringValue()).asIntVector();
    isMiner = std::find(minersList.begin(), minersList.end(), getIndex()) != minersList.end();
    if (isMiner) {
        EV << "Node " << getIndex() << " marked as miner" << std::endl;
    }
    blockSyncRecency = par("blockSyncRecency").intValue();
    coinbaseOutput = par("coinbaseOutput").intValue();

    messageGen = std::make_unique<MessageGenerator>(versionNumber);
}

void POWNode::scheduleSelfMessages() {
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_CHECK_QUEUES));
    scheduleAt(simTime() + dumpAddressesInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_DUMP_ADDRS));

    // initial address poll delayed to allow initial connections to be built up
    scheduleAt(simTime() + 2 * threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_POLL_ADDRS));

    if (isMiner) {
        scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_MINE));
    }
}

void POWNode::initialize() {
    internalInitialize();

    // set up step 1:
    // attempt to establish connections with the list of known nodes (default nodes if there are none)
    initConnections();

    // step 2:
    // set up self scheduled messages
    // just need to schedule one self message for each type because the handler will schedule the next one
    scheduleSelfMessages();

    // step 3:
    // send node version message on outbound connections
    auto msg = messageGen->generateVersionMessage(getIndex(), blockchain->chainHeight());

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
        startBlockSync(peerIndex);
        if (!peer->second->blocksToSend.empty()) {
            sendToNode(messageGen->generateBlocksMessage(getIndex(), peer->second->blocksToSend), peerIndex);
            peer->second->blocksToSend.clear();
        }
    } else {
        EV << "Attempted to send data to nonexistant node " << peerIndex << std::endl;
    }
}

void POWNode::startBlockSync(int peerIndex) {
    if (!state.syncStarted) {
        EV << "Starting block sync with peer " << peerIndex << std::endl;
        // request headers from a single peer, unless our best header is recent enough
        std::shared_ptr<Block> bestHeader = nullptr;
        if (blockchain->chainHeight() > 0) {
            bestHeader = blockchain->getTip();
        }
        if (state.numSyncs == 0 || bestHeader->getHeader().creationTime > simTime().inUnit(SimTimeUnit::SIMTIME_S) - blockSyncRecency) {
            state.syncStarted = true;
            state.numSyncs++;
            int64_t bestHash = BlockHeader::NULL_HASH;
            if (bestHeader) {
                bestHash = bestHeader->getHeader().hash;
                if (bestHeader->prevBlock()) {
                    bestHeader = bestHeader->prevBlock();
                    bestHash = bestHeader->getHeader().hash;
                }
            }
            sendToNode(messageGen->generateGetHeadersMessage(getIndex(), bestHash), peerIndex);
        }
    }
}

void POWNode::mineHandler(POWMessage *msg) {
    if (isMiner) {
        EV << "Handling mine message" << std::endl;
        // proof of work is handled by the scheduler
        // all we need to do is validate transactions
        if (blockchain->chainHeight() > 0) {
            for (auto tx : state.unverifiedTransactions) {
                if (std::all_of(tx.inputs.begin(), tx.inputs.end(), [&, this](TransactionInput in) {
                    return this->blockchain->getTip()->getTx()[in.prevTxHash].outputs[in.prevTxN].publicKey ==
                            in.signature - 1; // TODO: for now don't check for double spends
                })) {
                    EV_DETAIL << "Transaction valid" << std::endl;
                    state.verifiedTransactions.push_back(tx);
                } else {
                    EV_DETAIL << "Transaction invalid" << std::endl;
                }
            }
        }
        state.unverifiedTransactions.clear();
        scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_MINE));
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
    broadcastMessage(messageGen->generateMessage(meIndex, MessageGenerator::MESSAGE_GETADDR_COMMAND),
            [&, this](int peer){ return this->peers[peer]->flags.test(SuccessfullyConnected); });
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(meIndex, MessageGenerator::MESSAGE_POLL_ADDRS));
}

void POWNode::scheduleAddrAd(int peerIndex) {
    std::string data = "peerIndex=" + std::to_string(peerIndex);
    // same interval as threadSchedule since BTC does this task as part of that thread
    //scheduleAt(simTime() + poisson((double)(int64_t)threadScheduleInterval), messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_ADVERTISE_ADDRESSES, data));
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
    // do broadcasts
    sendBroadcasts();
    scheduleAt(simTime() + threadScheduleInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_CHECK_QUEUES));
}

void POWNode::sendBroadcasts() {
    // for now this is just block announcements
    broadcastMessage(messageGen->generateHeadersMessage(getIndex(), state.blocksToAnnounce),
            [&,this](int peerIndex) {
        return this->peers[peerIndex]->flags.test(SuccessfullyConnected) && !this->peers[peerIndex]->flags.test(Disconnect);
    });
    state.blocksToAnnounce.clear();
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
    scheduleAt(simTime() + dumpAddressesInterval, messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_DUMP_ADDRS));
}
#endif

#if(1) // handle incoming messages from peers
void POWNode::handleMessage(cMessage *msg) {
    std::string msgName = msg->getName();
    std::string test = "schedule";
    if (msgName == test) {
        SchedulerMessage *schMessage = check_and_cast<SchedulerMessage*>(msg);
        EV << "Received simulation scheduler message " << schMessage << std::endl;
        handleScheduledMessage(schMessage);
    } else {
        EV << "Not a simulation scheduled message" << std::endl;
        POWMessage *powMessage = check_and_cast<POWMessage *>(msg);
        logReceivedMessage(powMessage);
        if (powMessage->isSelfMessage()) {
            EV << "Received self scheduler message.  Sending to appropriate handler." << std::endl;
            handleSelfMessage(powMessage);
            delete msg;
        } else {
            int source = powMessage->getSource();
            EV << "Adding message to queue for peer " << source << std::endl;
            peers[source]->incomingMessages.push_back(powMessage);
            // don't delete here because the message needs to be processed
        }
    }
}

void POWNode::handleBlocksMessage(POWMessage *msg) {
    EV << "Handling blocks message " << msg << std::endl;
    BlocksMessage *blMsg = check_and_cast<BlocksMessage*>(msg);
    for (auto bl : blMsg->getBlocks()) {
        blockchain->addBlock(bl);
    }
    chainHeight = blockchain->chainHeight();
}

void POWNode::handleScheduledMessage(SchedulerMessage *msg) {
    EV_DETAIL << "Simulation scheduler handler contents for peer " << getIndex() << ":" << std::endl;
    for (auto pair : simulationScheduleHandlers) {
        EV_DETAIL << pair.first << std::endl;
    }
    EV << "Handling simulation scheduler message" << msg << std::endl;
    std::string methodName = msg->getMethod();
    auto handlerIt = simulationScheduleHandlers.find(methodName);
    if (handlerIt != simulationScheduleHandlers.end()) {
        handlerIt->second(*this, msg);
    } else {
        EV << "No handler for simulation scheduled message " << msg << " with method name " << methodName << std::endl;
    }
}

void POWNode::handleGetHeadersMessage(POWMessage *msg) {
    GetHeadersMessage *ghMsg = check_and_cast<GetHeadersMessage *>(msg);
    int meNode = getIndex();
    int sourceNode = ghMsg->getSource();
    EV << "Handling request for headers from " << sourceNode << std::endl;
    // TODO: find headers that sender does not know about in our chain
    std::vector<BlockHeader> toSend;
    auto newBlocks = blockchain->getBlocksAfter(ghMsg->getHash());
    for (auto block : newBlocks) {
        toSend.push_back(block->getHeader());
    }
    sendToNode(messageGen->generateHeadersMessage(meNode, toSend), sourceNode);
}

void POWNode::handleHeadersMessage(POWMessage *msg) {
    HeadersMessage *headersMsg = check_and_cast<HeadersMessage*>(msg);
    int meNode = getIndex();
    int sourceNode = msg->getSource();
    EV << "Handling headers received from " << sourceNode << std::endl;
    EV << "Received " << headersMsg->getHeaders().size() << " headers." << std::endl;
    int64_t hashLastBlock = BlockHeader::NULL_HASH;
    int64_t requestHeader = BlockHeader::NULL_HASH;
    bool foundOldest = false;
    for (auto header : headersMsg->getHeaders()) {
        if (hashLastBlock != BlockHeader::NULL_HASH && header.parentHash != hashLastBlock) {
            // non continuous headers sequence
            EV_WARN << "Received non continuous headers sequence from " << sourceNode << std::endl;
            return;
        }
        if (blockchain->chainHeight() == 0 || (!foundOldest && hashLastBlock == blockchain->getTip()->getHeader().hash)) {
            foundOldest = true;
            requestHeader = header.hash;
        }
        hashLastBlock = header.hash;
    }
    if (foundOldest) {
        sendToNode(messageGen->generateGetBlocksMessage(meNode, requestHeader), sourceNode);
    }
}

void POWNode::handleNodeVersionMessage(POWMessage *msg) {
    VersionMessage *versionMsg = check_and_cast<VersionMessage*>(msg);
    int sourceNode = versionMsg->getSource();
    int meNode = getIndex();
    int sourceVersionNo = versionMsg->getVersionNo();
    if (sourceVersionNo < minAcceptedVersionNumber) {
        // disconnect from nodes that are too old
        EV << "Node " << sourceNode << " using obsolete protocol version.  Minimum accepted version is " << minAcceptedVersionNumber << std::endl;
        sendToNode(messageGen->generateRejectMessage(meNode, true, "obsolete"), sourceNode);
        disconnectNode(sourceNode);
    } else {
        std::string bubbleMessage = "Received valid version message from " + std::to_string(sourceNode);
        bubble(bubbleMessage.c_str());
        bool sourceInbound = peers[sourceNode]->flags.test(Inbound);
        // TODO: store starting height of incoming node
        if (sourceInbound) {
            EV << "Sending node version message to inbound peer " << sourceNode << std::endl;
            sendToNode(messageGen->generateVersionMessage(meNode, blockchain->chainHeight()), sourceNode);
        }

        EV << "Node " << sourceNode << " is compatible.  Sending VERACK." << std::endl;
        peers[sourceNode]->version = sourceVersionNo;
        int sourceChainHeight = versionMsg->getChainHeight();
        if (sourceChainHeight > state.bestPeerHeight) {
            state.bestPeerHeight = sourceChainHeight;
            peers[sourceNode]->knownHeight = sourceChainHeight;
            if (sourceChainHeight > blockchain->chainHeight()) {
                peers[sourceNode]->flags.set(RequestHeaders);
            }
        }

        sendToNode(messageGen->generateMessage(meNode, MessageGenerator::MESSAGE_VERACK_COMMAND), sourceNode);

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
    int meIndex = getIndex();
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
    peers[sourceIndex]->flags.set(SuccessfullyConnected);
    if (peers[sourceIndex]->flags.test(RequestHeaders) && peers[sourceIndex]->knownHeight == state.bestPeerHeight) {
        sendToNode(messageGen->generateGetHeadersMessage(meIndex, blockchain->getTip()->getHeader().hash), sourceIndex);
    }
}

void POWNode::handleRejectMessage(POWMessage *msg) {
    RejectMessage *rejectMsg = check_and_cast<RejectMessage*>(msg);
    int source = rejectMsg->getSource();
    std::string bubbleMessage = "Received reject message from " + std::to_string(source);
    bubble(bubbleMessage.c_str());
    EV << "Reject reason: " << rejectMsg->getReason() << std::endl;
    if (rejectMsg->getDisconnect()) {
        disconnectNode(source);
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
    AddrsMessage *addrsMsg = check_and_cast<AddrsMessage*>(msg);
    int messageSource = addrsMsg->getSource();
    std::string bubbleMessage = "handling addrs message from peer: " + std::to_string(messageSource);
    bubble(bubbleMessage.c_str());
    EV << "Handling addrs message from peer " << messageSource << std::endl;
    std::vector<int> newAddresses = addrsMsg->getAddresses();
    EV << "Received " << newAddresses.size() << " addresses from node " << messageSource << std::endl;
    // TODO: attempt to connect to some if not all of the new peers
    dynamicConnect(newAddresses);
    addrMan->addAddresses(newAddresses);
}

void POWNode::handleTxMessage(POWMessage *msg) {
    // ignore a tx message if we are not a miner
    if (isMiner) {
        TxMessage *txMsg = check_and_cast<TxMessage*>(msg);
        state.unverifiedTransactions.push_back(txMsg->getTx());
    }
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

void POWNode::handleGetBlocksMessage(POWMessage *msg) {
    GetHeadersMessage *bhMessage = check_and_cast<GetHeadersMessage*>(msg);
    int messageSource = bhMessage->getSource();
    EV << "Handling getblocks message from " << messageSource << std::endl;
    auto newBlocks = blockchain->getBlocksAfter(bhMessage->getHash());
    std::copy(newBlocks.begin(), newBlocks.end(), std::back_inserter(peers[messageSource]->blocksToSend));
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
    sendToNode(messageGen->generateAddrsMessage(getIndex(), addresses), messageSource);
}
#endif

#if(1) // handle simulation scheduled messages
void POWNode::handleNewBlock(SchedulerMessage *msg) {
    if (!isMiner) {
        EV_ERROR << "Only miners can create new blocks" << std::endl;
        error("only miners can create new blocks");
    } else {
        int64_t prev = BlockHeader::NULL_HASH;
        if (blockchain->chainHeight() != 0) {
            prev = blockchain->getTip()->getHeader().hash;
        }
        auto result = Block::create(getIndex(), coinbaseOutput, prev, simTime().inUnit(SimTimeUnit::SIMTIME_S), state.verifiedTransactions);
        state.verifiedTransactions.clear();
        blockchain->addBlock(result);
        state.blocksToAnnounce.push_back(result->getHeader());
        chainHeight = blockchain->chainHeight();
    }
}

void POWNode::handleNewTx(SchedulerMessage *msg) {
    if (blockchain->chainHeight() > 0) {
        Transaction tx;
        int peer = msg->getParameters()[0];
        int amount = msg->getParameters()[1];
        TransactionOutput txOut;
        txOut.value = amount;
        txOut.publicKey = peer * 2;
        auto prevTxVec = blockchain->getTip()->getTx();
        std::vector<TransactionInput> inputs;
        for (auto prevTx : prevTxVec) {
            for (int i = 0; i < prevTx.outputs.size(); ++i) {
                if (prevTx.outputs[i].publicKey == getIndex() * 2) {
                    // the transaction output is ours, so check if we've spent the output yet
                    int outAmount = state.outputsSpent[prevTx.hash][i];
                    if (outAmount > 0) {
                        TransactionInput txIn;
                        if (outAmount > amount) {
                            state.outputsSpent[prevTx.hash][i] -= amount;
                            amount = 0;
                        } else {
                            amount -= outAmount;
                            state.outputsSpent[prevTx.hash][i] = 0;
                        }
                        txIn.prevTxHash = prevTx.hash;
                        txIn.prevTxN = i;
                        txIn.signature = getIndex() * 2 + 1;
                        inputs.push_back(txIn);
                    }
                }
            }
        }
        if (amount == 0) {
            tx.outputs.push_back(txOut);
            std::copy(inputs.begin(), inputs.end(), std::back_inserter(tx.inputs));
            tx.hash = blockchain->getMaxTxHash() + 1;
            broadcastMessage(messageGen->generateTxMessage(getIndex(), tx), [&,this](int peerIndex) {
                return this->peers[peerIndex]->flags.test(SuccessfullyConnected) && !this->peers[peerIndex]->flags.test(Disconnect);
            });
        } // currently no check that we aren't overspending, but we won't let that happen
    }
}
#endif

#if(1) // utility function

void POWNode::dynamicConnect(const std::vector<int> &newAddresses) {
    EV << "Dynamically connecting to new addresses." << std::endl;
    std::vector<int> toAdd;
    std::remove_copy_if(newAddresses.begin(), newAddresses.end(),
            std::back_inserter(toAdd), [&, this](int peer){ return this->nodeIndexToGateMap.find(peer) != this->nodeIndexToGateMap.end(); });
    // connect to half the peers to even out the number of inbound and outbound connections for each node
    int newCount = 0;
    for (int i = 0; i < toAdd.size() / 2; ++i) {
        ++newCount;
        int otherIndex = toAdd[i];
        connectTo(otherIndex, getPeerNodeByPath(otherIndex));
    }
    EV_DETAIL << "Dynamically connected to " << newCount << " of " << newAddresses.size() << " advertised peers." << std::endl;
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

void POWNode::refreshDisplay() const {
    char buf[40];
    sprintf(buf, "chainheight: %ld", chainHeight);
    getDisplayString().setTagArg("t", 0, buf);
}
#endif
