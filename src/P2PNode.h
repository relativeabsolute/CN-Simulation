/*
 * P2PNode.h
 *
 *  Created on: Nov 1, 2018
 *      Author: jburke
 */

#ifndef P2PNODE_H_
#define P2PNODE_H_

#include <string.h>
#include <omnetpp.h>
#include "p2p_msg_m.h"

using namespace omnetpp;

class P2PNode : public cSimpleModule {
public:
    P2PNode();
    virtual ~P2PNode();
protected:
    virtual void initialize() override;
    virtual void forwardMessage(P2P_Msg *msg);
    virtual void handleMessage(cMessage *msg) override;
    virtual P2P_Msg *generateNewMessage();
};

Define_Module(P2PNode)

#endif /* P2PNODE_H_ */
