/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"
#include <memory>

POWNode::POWNode() : messageGen(nullptr) {
}

POWNode::~POWNode() {
}

void POWNode::addNodeToGateMapping(int nodeIndex, cGate *gate) {
    EV << "Mapping node " << nodeIndex << " to gate " << gate << std::endl;
    nodeIndexToGateMap[nodeIndex] = gate;
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
                // TODO: put limit on number of gates/connections?
                cGate *destGateIn, *destGateOut;
                toCheck->getOrCreateFirstUnconnectedGatePair("gate", false, true, destGateIn, destGateOut);

                cGate *srcGateIn, *srcGateOut;
                getOrCreateFirstUnconnectedGatePair("gate", false, true, srcGateIn, srcGateOut);

                // TODO: may want to store the connections returned by connectTo here
                srcGateOut->connectTo(destGateIn);
                EV << "Outbound connection established" << std::endl;
                addNodeToGateMapping(*it, srcGateOut);
                destGateOut->connectTo(srcGateIn);
                EV << "Inbound connection established" << std::endl;
                toCheck->addNodeToGateMapping(getIndex(), destGateOut);
            }
        }
    }
    delete[] path;
}

void POWNode::setupMessageHandlers() {
    messageHandlers[MessageGenerator::MESSAGE_NODE_VERSION_COMMAND] = &POWNode::handleNodeVersionMessage;
    messageHandlers[MessageGenerator::MESSAGE_VERACK_COMMAND] = &POWNode::handleVerackMessage;
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
}

void POWNode::readConstantParameters() {
    versionNumber = par("version").intValue();
    minAcceptedVersionNumber = par("minAcceptedVersion").intValue();

    messageGen = new MessageGenerator(versionNumber);
}

void POWNode::initialize() {
    internalInitialize();

    // set up step 1:
    // attempt to establish connections with the list of default nodes
    // TODO: have node check for its data file, and if there is one read list of known nodes from it
    initConnections();

    // step 2:
    // send node version message on outbound connections
    auto msg = messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_NODE_VERSION_COMMAND, "");
    broadcastMessage(msg);

    /*
    for (auto mapIterator = nodeIndexToGateMap.begin(); mapIterator != nodeIndexToGateMap.end(); ++mapIterator) {
        // it->first is the index of the destination node
        // it->second is the gate index to send over
        auto newMsg = generateGetKnownNodesMessage();
        EV << newMsg << std::endl;
        send(newMsg, mapIterator->second);
    }
    */
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

void POWNode::handleMessage(cMessage *msg) {
    EV << "Received message.  Sending to appropriate handler." << std::endl;
    POWMessage *powMessage = check_and_cast<POWMessage *>(msg);
    std::string methodName = powMessage->getName();

    auto handlerIt = messageHandlers.find(methodName);
    if (handlerIt != messageHandlers.end()) {
        handlerIt->second(*this, powMessage);
    } else {
        EV << "Node has no handler for message of type " << methodName << std::endl;
    }
    delete msg;
}

/*
POWMessage *POWNode::generateGetNodeStatusMessage() {
    POWMessage *result = new POWMessage("getnodestatus");
    result->setSrc(getIndex()); // this message does not need to carry any data yet
    return result;
}

POWMessage *POWNode::generateGetKnownNodesMessage() {
    POWMessage *msg = new POWMessage("getknownnodes");
    msg->setSrc(getIndex());

    // TODO: fix this to send over known nodes read from file
    msg->setData("<data><known_nodes></known_nodes></data>");

    return msg;
}
*/

void POWNode::handleNodeVersionMessage(POWMessage *msg) {
    logReceivedMessage(msg);
    if (msg->getVersionNo() < minAcceptedVersionNumber) {
        // disconnect from nodes that are too old
        EV << "Node " << msg->getSource() << " using obsolete protocol version.  Minimum accepted version is " << minAcceptedVersionNumber << std::endl;
        sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_REJECT_COMMAND, "reason=obsolete"), msg->getSource());
    } else {
        EV << "Node " << msg->getSource() << " is compatible.  Sending VERACK." << std::endl;
        sendToNode(messageGen->generateMessage(getIndex(), MessageGenerator::MESSAGE_VERACK_COMMAND, ""), msg->getSource());
        // TODO: advertise our address if the connection is outbound
    }
}

void POWNode::handleVerackMessage(POWMessage *msg) {
    logReceivedMessage(msg);
    // TODO: update node state for source node if this is an outbound connection
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
