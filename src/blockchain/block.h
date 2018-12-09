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
#include <iostream>
#include <vector>
#include "tx.h"

typedef std::vector<Transaction>::size_type txs_size;

struct BlockHeader {
    int64_t hash;
    int64_t parentHash;
    txs_size numTx;

    friend std::istream &operator>>(std::istream &input, BlockHeader &header) {
        input >> header.hash >> header.parentHash >> header.numTx;
        return input;
    }

    friend std::ostream &operator<<(std::ostream &output, BlockHeader &header) {
        output << header.hash << header.parentHash << header.numTx;
        return output;
    }
};

class Block {
public:
    friend std::istream &operator>>(std::istream &input, Block &block) {
        fileReader >> block.header;
        for (txs_size i = 0; i < block.header->numTx; ++i) {
            Transaction temp;
            fileReader >> temp;
            block.transactions.push_back(temp);
        }
    }

    friend std::ostream &operator<<(std::ostream &output, const Block &block) {
        output << block.header;
        for (auto tx : block.transactions) {
            output << tx;
        }
    }

    std::unique_ptr<BlockHeader> getHeader() const {
        return header;
    }
private:
    std::unique_ptr<BlockHeader> header;
    std::vector<Transaction> transactions;
};

#endif /* BLOCKCHAIN_BLOCK_H_ */
