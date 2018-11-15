/*
 * POWNode.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#include "POWNode.h"
#include <memory>

POWNode::POWNode() {
    // TODO Auto-generated constructor stub

}

POWNode::~POWNode() {
    // TODO Auto-generated destructor stub
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
            EV << "Node " << path << " is ";
            if (!toCheck->isOnline()) {
                EV << "not ";
            }
            EV << "online." << std::endl;
        }
    }
    delete[] path;
}

bool POWNode::isOnline() const {
    return par("online").boolValue();
}

void POWNode::handleMessage(cMessage *msg) {

}
