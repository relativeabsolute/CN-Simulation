/*
 * POWNode.h
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#ifndef POWNODE_H_
#define POWNODE_H_

#include <omnetpp.h>
#include "P2PNode.h"

using namespace omnetpp;

class POWNode: public P2PNode {
public:
    POWNode();
    virtual ~POWNode();

    cGate *getNextAvailableGate(cGate::Type half);
protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    bool isOnline() const;
private:
    int nextGate;
};

Define_Module(POWNode)

#endif /* POWNODE_H_ */
