/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"
#include <memory>

POWNode::POWNode() : nextGate(0) {

}

POWNode::~POWNode() {
}

void POWNode::initialize() {
    // set up step 1:
    // attempt to establish connections with the list of default nodes
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

                srcGateOut->connectTo(destGateIn);
                EV << "Outbound connection established" << std::endl;
                destGateOut->connectTo(srcGateIn);
                EV << "Inbound connection established" << std::endl;
            }
        }
    }
    delete[] path;
}

bool POWNode::isOnline() const {
    return par("online").boolValue();
}

void POWNode::handleMessage(cMessage *msg) {

}

cGate *POWNode::getNextAvailableGate(cGate::Type half) {
    if (nextGate < gateSize("gate")) {
        return gateHalf("gate", half, nextGate++);
    } else {
        return nullptr;
    }
}
