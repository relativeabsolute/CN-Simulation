/*
 * Block.h
 *
 *  Created on: Dec 9, 2018
 *      Author: jburke
 */

#ifndef BLOCKCHAIN_BLOCK_H_
#define BLOCKCHAIN_BLOCK_H_

#include <omnetpp.h>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include "tx.h"

typedef std::vector<Transaction>::size_type txs_size;

struct BlockHeader {
    static const int64_t NULL_HASH = 0;

    int64_t hash;
    int64_t parentHash;
    txs_size numTx;
    int creationTime;

    BlockHeader() : hash(NULL_HASH), parentHash(NULL_HASH), numTx(0), creationTime(-1) {}

    friend std::istream &operator>>(std::istream &input, BlockHeader &header) {
        input >> header.hash >> header.parentHash >> header.numTx >> header.creationTime;
        return input;
    }

    friend std::ostream &operator<<(std::ostream &output, const BlockHeader &header) {
        output << header.hash << header.parentHash << header.numTx << header.creationTime;
        return output;
    }
};

class Block {
public:
    Block() {}

    friend std::istream &operator>>(std::istream &input, Block &block) {
        input >> block.header;
        for (txs_size i = 0; i < block.header.numTx; ++i) {
            Transaction temp;
            input >> temp;
            block.transactions.insert(std::make_pair(temp.hash, temp));
        }
        return input;
    }

    friend std::ostream &operator<<(std::ostream &output, const Block &block) {
        output << block.header;
        for (auto tx : block.transactions) {
            output << tx.second;
        }
        return output;
    }

    BlockHeader getHeader() const {
        return header;
    }

    void addTransaction(const Transaction &tx) {
        transactions[tx.hash] = tx;
        header.numTx++;
    }

    static Block create(int miner, int coinbaseHash, int reward, int64_t parentHash, int time) {
        Block result;
        result.header.creationTime = time;
        result.header.parentHash = parentHash;
        result.header.hash = parentHash + 1;
        result.header.numTx = 1;
        Transaction coinbase;
        TransactionInput txIn;
        txIn.prevTxHash = TransactionInput::COINBASE_HASH;
        txIn.prevTxN = TransactionInput::COINBASE_N;
        txIn.signature = 0;
        coinbase.inputs.push_back(txIn);
        TransactionOutput txOut;
        txOut.publicKey = miner * 2;
        txOut.value = reward;
        coinbase.outputs.push_back(txOut);
        coinbase.hash = coinbaseHash;
        result.transactions[coinbaseHash] = coinbase;
        return result;
    }

    std::string to_string() const {
        std::stringstream strBuf;
        strBuf << "Block header: " << std::endl <<
                "\tHash = " << header.hash << std::endl <<
                "\tParent hash = " << header.parentHash << std::endl <<
                "\tCreation time = " << header.creationTime << std::endl <<
                "\tNum tx = " << header.numTx << std::endl <<
                "Transactions:" << std::endl;
        for (auto txPair : transactions) {
            strBuf << "\tInputs:" << std::endl;
            for (auto txIn : txPair.second.inputs) {
                strBuf << "\t\tPrevious tx hash: " << txIn.prevTxHash << std::endl <<
                        "\t\tPrevious output index: " << txIn.prevTxN << std::endl <<
                        "\t\tSignature: " << txIn.signature << std::endl;
            }
            strBuf << "\tOutputs:" << std::endl;
            for (auto txOut : txPair.second.outputs) {
                strBuf << "\t\tPublic key: " << txOut.publicKey << std::endl <<
                        "\t\tValue: " << txOut.value << std::endl;
            }
        }
        return strBuf.str();
    }

    std::map<int64_t, Transaction> getTx() const { return transactions; }
private:
    BlockHeader header;
    std::map<int64_t, Transaction> transactions;
};

#endif /* BLOCKCHAIN_BLOCK_H_ */
