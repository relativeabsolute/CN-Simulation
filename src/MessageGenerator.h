/*
 * MessageGenerator.h
 *
 *  Created on: Nov 24, 2018
 *      Author: jburke
 */

#ifndef MESSAGEGENERATOR_H_
#define MESSAGEGENERATOR_H_

#include "messages/messages.h"
#include <utility>
#include <string>
#include <map>
#include <bitset>
#include "blockchain/tx.h"
#include "blockchain/block.h"

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
    static constexpr const char *MESSAGE_DUMP_ADDRS = "dumpaddr";
    static constexpr const char *MESSAGE_POLL_ADDRS = "polladdrs";
    static constexpr const char *MESSAGE_MINE = "mine";
    static constexpr const char *MESSAGE_NODE_VERSION_COMMAND = "nodeversion";
    static constexpr const char *MESSAGE_REJECT_COMMAND = "reject";
    static constexpr const char *MESSAGE_VERACK_COMMAND = "verack";
    static constexpr const char *MESSAGE_GETADDR_COMMAND = "getaddr";
    static constexpr const char *MESSAGE_ADDRS_COMMAND = "addrs";
    static constexpr const char *MESSAGE_ADDR_COMMAND = "addr";
    static constexpr const char *MESSAGE_TX_COMMAND = "tx";
    static constexpr const char *MESSAGE_GETHEADERS_COMMAND = "getheaders";
    static constexpr const char *MESSAGE_HEADERS_COMMAND = "headers";
    static constexpr const char *MESSAGE_GETBLOCKS_COMMAND = "getblocks";
    static constexpr const char *MESSAGE_BLOCKS = "blocks";

    explicit MessageGenerator(int versionNo) : versionNo(versionNo) {
        initMessageScopes();
    }

    /*! Generate a new message with the given parameter.
     * \param sourceIndex Index of node sending the message.
     * \param command Type of message to send (also used as message name)
     * \param data Optional string of data to attach to message.  Use empty string for messages that do not need data.
     */
    template <typename MessageType = POWMessage>
    MessageType *generateMessage(int sourceIndex, std::string command) {
        MessageType *result = new MessageType(std::move(command).c_str());
        result->setSource(sourceIndex);
        result->setVersionNo(versionNo);
        return result;
    }

    VersionMessage *generateVersionMessage(int sourceIndex, int chainHeight) {
        auto result = generateMessage<VersionMessage>(sourceIndex, MESSAGE_NODE_VERSION_COMMAND);
        result->setChainHeight(chainHeight);
        return result;
    }

    GetHeadersMessage *generateGetHeadersMessage(int sourceIndex, int64_t hash) {
        auto result = generateMessage<GetHeadersMessage>(sourceIndex, MESSAGE_GETHEADERS_COMMAND);
        result->setHash(hash);
        return result;
    }

    GetHeadersMessage *generateGetBlocksMessage(int sourceIndex, int64_t hash) {
        auto result = generateMessage<GetHeadersMessage>(sourceIndex, MESSAGE_GETBLOCKS_COMMAND);
        result->setHash(hash);
        return result;
    }

    BlocksMessage *generateBlocksMessage(int sourceIndex, const std::vector<std::shared_ptr<Block>> blocks) {
        auto result = generateMessage<BlocksMessage>(sourceIndex, MESSAGE_GETBLOCKS_COMMAND);
        result->setBlocks(blocks);
        return result;
    }

    TxMessage *generateTxMessage(int sourceIndex, const Transaction &tx) {
        auto result = generateMessage<TxMessage>(sourceIndex, MESSAGE_TX_COMMAND);
        result->setTx(tx);
        return result;
    }

    RejectMessage *generateRejectMessage(int sourceIndex, bool disconnect, const std::string &reason) {
        auto result = generateMessage<RejectMessage>(sourceIndex, MESSAGE_REJECT_COMMAND);
        result->setReason(reason.c_str());
        result->setDisconnect(disconnect);
        return result;
    }

    HeadersMessage *generateHeadersMessage(int sourceIndex, const std::vector<BlockHeader> &headers) {
        auto result = generateMessage<HeadersMessage>(sourceIndex, MESSAGE_HEADERS_COMMAND);
        result->setHeaders(headers);
        return result;
    }

    AddrsMessage *generateAddrsMessage(int sourceIndex, const std::vector<int> &addrs) {
        auto result = generateMessage<AddrsMessage>(sourceIndex, MESSAGE_ADDRS_COMMAND);
        result->setAddresses(addrs);
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
        messageScopes.insert(std::make_pair(MESSAGE_ADDRS_COMMAND, "00"));  // addrs sent in response to get addr, so same scope
        messageScopes.insert(std::make_pair(MESSAGE_GETHEADERS_COMMAND, "00"));
        messageScopes.insert(std::make_pair(MESSAGE_HEADERS_COMMAND, "00"));
        messageScopes.insert(std::make_pair(MESSAGE_TX_COMMAND, "00"));
    }

    int versionNo;
    std::map<std::string, std::bitset<NumMessageScopes>> messageScopes;
};

#endif /* MESSAGEGENERATOR_H_ */
