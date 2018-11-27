/*
 * MessageGenerator.h
 *
 *  Created on: Nov 24, 2018
 *      Author: jburke
 */

#ifndef MESSAGEGENERATOR_H_
#define MESSAGEGENERATOR_H_

#include "pow_message_m.h"
#include <utility>
#include <string>
#include <map>
#include <bitset>

enum MessageScope {
    PreVersion,
    PreVerack,
    NumMessageScopes
};

/*! Utility class that makes generating messages easier.
 *
 */
class MessageGenerator {
public:
    static constexpr const char *MESSAGE_CHECK_QUEUES = "checkqueues"; // self message to simulate BTC's threading.  does not need a scope
    static constexpr const char *MESSAGE_ADVERTISE_ADDRESSES = "advertiseaddrs";
    static constexpr const char *MESSAGE_NODE_VERSION_COMMAND = "nodeversion";
    static constexpr const char *MESSAGE_REJECT_COMMAND = "reject";
    static constexpr const char *MESSAGE_VERACK_COMMAND = "verack";
    static constexpr const char *MESSAGE_GETADDR_COMMAND = "getaddr";
    static constexpr const char *MESSAGE_ADDR_COMMAND = "addr";

    explicit MessageGenerator(int versionNo) : versionNo(versionNo) {
        initMessageScopes();
    }

    /*! Generate a new message with the given parameter.
     * \param sourceIndex Index of node sending the message.
     * \param command Type of message to send (also used as message name)
     * \param data Optional string of data to attach to message.  Use empty string for messages that do not need data.
     */
    POWMessage *generateMessage(int sourceIndex, std::string command, std::string data) {
        POWMessage *result = new POWMessage(std::move(command).c_str());
        result->setSource(sourceIndex);
        result->setData(std::move(data).c_str());
        result->setVersionNo(versionNo);
        return result;
    }

    bool messageInScope(const std::string &msgName, MessageScope scope) const {
        return messageScopes.at(msgName).test(scope);
    }
private:
    void initMessageScopes() {
        messageScopes.insert(std::make_pair(MESSAGE_NODE_VERSION_COMMAND, "11")); // version command first accepted command
        messageScopes.insert(std::make_pair(MESSAGE_VERACK_COMMAND, "10")); // verack accepted after version command
        messageScopes.insert(std::make_pair(MESSAGE_REJECT_COMMAND, "11")); // reject can be sent at any time
        messageScopes.insert(std::make_pair(MESSAGE_GETADDR_COMMAND, "00"));  // get addr sent in response to version
    }

    int versionNo;
    std::map<std::string, std::bitset<NumMessageScopes>> messageScopes;
};

#endif /* MESSAGEGENERATOR_H_ */
