/*
 * pow_node_queues.h
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#ifndef POW_NODE_DATA_H_
#define POW_NODE_DATA_H_

#include <map>
#include <deque>
#include <bitset>
#include "pow_message_m.h"

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
     * We only use queue behavior, but having a deque allows convenience when inserting addresses.
     */
    std::deque<int> addresses;

    std::deque<POWMessage *> incomingMessages;

    std::bitset<NumFlags> flags;

    int version;
};

#endif /* POW_NODE_DATA_H_ */
