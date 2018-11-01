/*
 * P2PNode.cpp
 *
 *  Created on: Nov 1, 2018
 *      Author: jburke
 */

#include "P2PNode.h"
#include <stdio.h>

P2PNode::P2PNode() {
}

P2PNode::~P2PNode() {
}

void P2PNode::initialize() {
    if (getIndex() == 0) {
        P2P_Msg *message = generateNewMessage();
        scheduleAt(0.0, message);
    }
}

void P2PNode::handleMessage(cMessage *msg) {
    P2P_Msg *_msg = check_and_cast<P2P_Msg *>(msg);

    if (_msg->getDestination() == getIndex()) {
        EV << "Message " << _msg << " arrived after " << _msg->getHopCount() << " hops.\n";
        bubble("ARRIVED, starting new one!");
        delete _msg;

        EV << "Generating another message: ";
        P2P_Msg *new_msg = generateNewMessage();
        EV << new_msg << endl;
        forwardMessage(new_msg);
    } else {
        forwardMessage(_msg);
    }
}

P2P_Msg *P2PNode::generateNewMessage() {
    int src = getIndex();
    int n = getVectorSize();
    int dest = intuniform(0, n-2);
    if (dest >= src) {
        dest++;
    }

    char msg_name[20];
    sprintf(msg_name, "msg-%d-to-%d", src, dest);

    P2P_Msg *msg = new P2P_Msg(msg_name);
    msg->setSource(src);
    msg->setDestination(dest);

    return msg;
}

void P2PNode::forwardMessage(P2P_Msg *msg) {
    msg->setHopCount(msg->getHopCount() + 1);

    int n = gateSize("gate");
    int k = intuniform(0, n-1);

    EV << "Forwarding message " << msg << " on gate[" << k << "]\n";
    send(msg, "gate$o", k);
}
