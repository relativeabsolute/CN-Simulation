/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"


POWNode::POWNode() {
}

POWNode::~POWNode() {
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
                destGateOut->connectTo(srcGateIn);
                EV << "Inbound connection established" << std::endl;
            }
        }
    }
    delete[] path;
}

void POWNode::initialize() {
    // set up step 1:
    // attempt to establish connections with the list of default nodes
    // TODO: have node check for its data file, and if there is one read list of known nodes from it
    initConnections();

    // step 2:
    // ask nodes for their list of known nodes (and indicate that we only know default nodes)
    // we do this by scheduling a get node neighbors message

    // TODO: iterate over nodes and send a new getknownnodes message
    // need to handle mapping of nodes to gates (since connection to node n is not necessarily
    // over gate index n
}

bool POWNode::isOnline() const {
    return par("online").boolValue();
}

void POWNode::handleMessage(cMessage *msg) {
    POWMsg *powMsg = check_and_cast<POWMsg*>(msg);

}

POWMsg *POWNode::generateGetKnownNodesMessage(int dest) {
    POWMsg *msg = new POWMsg("getknownnodes");
    msg->setSource(getIndex());

    // TODO: fix this to send over known nodes read from file
    msg->setData("<data><known_nodes></known_nodes></data>");

    return msg;
}
