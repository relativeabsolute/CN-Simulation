/*
 * pow_node_queues.h
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#ifndef POW_NODE_DATA_H_
#define POW_NODE_DATA_H_

#include <map>
#include <set>
#include <deque>
#include <bitset>
#include "pow_message_m.h"

// TODO: may want to separate flags to be stored by node and by node state
enum POWNodeFlags {
    HasGetAddr,
    SuccessfullyConnected,
    Inbound,
    HasSentAddr,
    Disconnect,
    PauseSend,
    PauseReceive,
    NumFlags
};

/*! Structure representing data a node knows about a peer.
 */
struct POWNodeData {
    /*! Contains addresses of peers to be advertised to the peer.
     */
    std::set<int> addressesToBeSent;

    std::deque<POWMessage *> incomingMessages;

    std::bitset<NumFlags> flags;

    std::set<int> knownAddresses;

    int version;
};

#endif /* POW_NODE_DATA_H_ */
