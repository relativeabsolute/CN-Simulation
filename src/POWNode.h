/*
 * POWNode.h
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#ifndef POWNODE_H_
#define POWNODE_H_

#include <map>
#include <omnetpp.h>
#include "P2PNode.h"
#include "pow_message_m.h"
#include "MessageGenerator.h"
#include "pow_node_data.h"
#include "addr_manager.h"
#include <functional>
#include <memory>
#include <queue>

using namespace omnetpp;

/*! Node in a proof of work based blockchain network (i.e., Bitcoin, Litecoin, etc).
 *
 */
// TODO: set some nodes to only accept incoming connections, instead of attempting to connect to all known nodes at beginning
class POWNode: public P2PNode {
public:
    POWNode();
    virtual ~POWNode();
protected:
    /*! Initialize the node.  Occurs during the set up stage of the simulation, before any messages are sent.
     * Runs the following steps:
     * 1.  Establish connections with nodes in known node list (if no known nodes, just connect to a list of default nodes).
     * 2.  Broadcast our known nodes list (excluding default nodes)
     */
    virtual void initialize() override;

    /*! Process an incoming message.  Message gets added to incomingMessages queue, which gets processed at a user-specified
     * time interval.
     * \param msg Message to handle.  Will be cast to a POWMessage.
     */
    virtual void handleMessage(cMessage *msg) override;

    /*! Check if the node is online (would be handled by TCP timeouts in real network).  Connections will not be established with an offline node.
     * \returns True if the node is online and can be connected to, false otherwise.
     */
    bool isOnline() const;

    /*! Map the node index to the outgoing gate to be used to send messages over.
     * \param nodeIndex The intended destination index for messages
     * \param gate The outgoing gate to send messages over.
     */
    void addNodeToGateMapping(int nodeIndex, cGate *gate);
private:
    /*! Internal initialzation steps, done before anything network related.
     * These involve loading data files, connecting message handlers, etc.
     */
    void internalInitialize();

    /*! Read NED parameters that will not change during the simulation.
     *
     */
    void readConstantParameters();

    /*! Create connections to currently known list of networks (step 1 of initialization).  Each connection will create a new gate in the source and destination nodes if needed.
     * In the future, the max number of connections may be a parameter in the network simulation (as the actual Bitcoin client has such a parameter).
     */
    void initConnections();

    /*! Connect message handlers using the name to member function map.
     *
     */
    void setupMessageHandlers();

    /*! Initiate the appropriate "thread" for the given name.
     * \param methodName Name of thread to invoke.
     */
    void handleSelfMessage(const std::string &methodName);

    /*! Send given message to all currently known peers.  Copies the message for each one.
     * \param broadcast Message to broadcast.
     */
    void broadcastMessage(POWMessage *broadcast);

    /*! Handle an incoming node version message.  If the node's version is less than our minimum acceptable
     * protocol version, send a reject message in response, otherwise, send a verack message in response.
     * \param msg Message to handle.
     */
    void handleNodeVersionMessage(POWMessage *msg);

    /*! Handle an incoming verack message.  Set state of connection to accept incoming blockchain messages.
     * \param msg Message to handle.
     */
    void handleVerackMessage(POWMessage *msg);

    /*! Handle an incoming reject message.  Disconnect from the sender if the data indicates so.
     * \param msg Message to handle.  Contains data that indicates the ban's reason and whether it should result in a disconnect.
     */
    void handleRejectMessage(POWMessage *msg);

    /*! Handle an incoming get addr message.  Push our known addresses into the corresponding node's
     * addresses queue.
     * \param msg Message to handle.  Does not contain any data.
     */
    void handleGetAddrMessage(POWMessage *msg);

    /*! Print information upon receiving a message.
     * \param msg Message to log.
     */
    void logReceivedMessage(POWMessage *msg) const;

    /*! Convenience method for sending a message to the specified node.
     * \param msg Message to send
     * \param nodeIndex Index of node to send to.
     */
    void sendToNode(POWMessage *msg, int nodeIndex);

    /*! Convert a data string from a message to a map.
     * \param messageData string of data to convert.  Formatted by param1key=param1value,param2key=param2value,etc.
     * \returns Map containing key value pairs for each data parameter
     */
    std::map<std::string, std::string> dataToMap(const std::string &messageData) const;

    /*! "Thread" that checks peers' incoming and outgoing message queues.
     * Calls processIncomingMessages and sendOutgoingMessages for each peer.
     */
    void messageHandler();

    /*! Process a message from a queue.
     * Calls the message's appropriate handler, if one exists.
     */
    void processMessage(POWMessage *msg);

    /*! Process the given peer's incoming messages.
     * \param peerIndex Index of peer to process messages of.
     * \returns True if there is still more processing to be done for the peer, false otherwise.
     */
    bool processIncomingMessages(int peerIndex);

    void sendOutgoingMessages(int peerIndex);

    /*! Disconnect from the specified node.
     * \param nodeIndex Index of node to disconnect
     */
    void disconnectNode(int nodeIndex);

    /*! Part of node loop run post initialization.
     * Sends any necessary data to peers.
     */
    void sendMessages();

    /*! Checks if the given message is being received at an appropriate time.
     * \param msg Message to check.
     * \returns True if the message is in scope, false otherwise.
     */
    bool checkMessageInScope(POWMessage *msg);

    void connectTo(int otherIndex, POWNode *other);

    std::map<std::string, std::function<void(POWNode &, POWMessage *)> > messageHandlers;
    std::map<std::string, std::function<void(POWNode &)> > selfMessageHandlers;
    std::map<int, cGate*> nodeIndexToGateMap;
    int versionNumber;
    int minAcceptedVersionNumber;
    int maxMessageProcess;
    std::unique_ptr<MessageGenerator> messageGen;
    // maintain data known about each peer
    std::map<int, std::unique_ptr<POWNodeData> > peers;
    std::unique_ptr<AddrManager> addrMan;
    std::queue<int> peersProcess; // make sure nodes are processed fairly
    simtime_t threadScheduleInterval;
    int currentMessagesProcessed;  // counter for number of messages that have been processed in one loop
};

Define_Module(POWNode)

#endif /* POWNODE_H_ */
