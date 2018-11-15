/*
 * P2PRandomTopologyNode.h
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#ifndef P2PRANDOMTOPOLOGYNODE_H_
#define P2PRANDOMTOPOLOGYNODE_H_

#include "P2PNode.h"
#include "p2p_msg_m.h"

class P2PRandomTopologyNode: public P2PNode {
public:
    P2PRandomTopologyNode();
    virtual ~P2PRandomTopologyNode();
protected:
    virtual void initialize() override;
    virtual void forwardMessage(P2P_Msg *msg);
    virtual void handleMessage(cMessage *msg) override;
    virtual P2P_Msg *generateNewMessage();
};

Define_Module(P2PRandomTopologyNode)

#endif /* P2PRANDOMTOPOLOGYNODE_H_ */
