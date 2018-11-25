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

/*! Utility class that makes generating messages easier.
 *
 */
class MessageGenerator {
public:
    static constexpr const char *MESSAGE_NODE_VERSION_COMMAND = "nodeversion";
    static constexpr const char *MESSAGE_REJECT_COMMAND = "reject";
    static constexpr const char *MESSAGE_VERACK_COMMAND = "verack";

    explicit MessageGenerator(int versionNo) : versionNo(versionNo) {}

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

private:
    int versionNo;
};

#endif /* MESSAGEGENERATOR_H_ */
