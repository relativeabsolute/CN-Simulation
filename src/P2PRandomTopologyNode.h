/*
 * P2PRandomTopologyNode.h
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#ifndef P2PRANDOMTOPOLOGYNODE_H_
#define P2PRANDOMTOPOLOGYNODE_H_

#include "P2PNode.h"
#include "messages/p2p_msg_m.h"

/*! Basic implementation of Peer to Peer node.
 * Results in a randomly interconnected peer to peer network, where messages are forwarded to destination
 * nodes until the simulation ends.
 */
class P2PRandomTopologyNode: public P2PNode {
public:
    P2PRandomTopologyNode();
    virtual ~P2PRandomTopologyNode();
protected:
    /*! Initializes the peer to peer network.
     * If this is the first node in the network, creates a new message and schedules it to be sent
     * at the beginning of the simulation.
     */
    virtual void initialize() override;

    /*! Forwards the given message to a random node in the network.
     * \param msg Message containing source node, destination node, and number of hops the message has traveled.
     */
    virtual void forwardMessage(P2P_Msg *msg);

    /*! Process an incoming message.
     * Check if we are the message's destination: if we are, we indicate such on the log, then generate a new message
     * and repeat the process.  Otherwise, we simply forward the message.
     * \param msg Message to process.
     */
    virtual void handleMessage(cMessage *msg) override;

    /*! Create a new message with a randomly generated destination.
     * \returns Message to be passed along until reaching randomly generated destination node.
     */
    virtual P2P_Msg *generateNewMessage();
};

Define_Module(P2PRandomTopologyNode)

#endif /* P2PRANDOMTOPOLOGYNODE_H_ */
