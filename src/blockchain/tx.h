/*
 * Tx.h
 *
 *  Created on: Dec 9, 2018
 *      Author: jburke
 */

#ifndef BLOCKCHAIN_TX_H_
#define BLOCKCHAIN_TX_H_

#include <map>
#include <iostream>
#include <limits>

struct TransactionInput {
    int prevTxHash; // identifier of transaction leading to this one
    int prevTxN; // index of specific output within previous transaction

    static constexpr int COINBASE_HASH = 0;
    static constexpr int COINBASE_N = std::numeric_limits<int>::max();

    // if this is a coinbase then this will contain the block height
    int signature; // we are sort of implementing BTC's Pay2PK tx type
    // Pay2PK involves outputs providing a "challenge" which involves outputs asking for the
    // ownership of the public key corresponding to the public key they provide
    // inputs are valid if they provide this private key

    /*! Coinbase transaction inputs are ones that create a new coin.
     * They are identified by the hash and n values containing special values.
     */
    bool isCoinbase() {
        return prevTxHash == COINBASE_HASH && prevTxN == COINBASE_N;
    }

    friend std::ostream &operator<<(std::ostream &outputStream, const TransactionInput &txIn) {
        outputStream << txIn.prevTxHash << txIn.prevTxN << txIn.signature;
        return outputStream;
    }

    friend std::istream &operator>>(std::istream &inputStream, TransactionInput &txIn) {
        inputStream >> txIn.prevTxHash >> txIn.prevTxN >> txIn.signature;
        return inputStream;
    }
};

struct TransactionOutput {
    int value; // amount of "cents" of our currency
    int publicKey;

    friend std::ostream &operator<<(std::ostream &outputStream, const TransactionOutput &txOut) {
        outputStream << txOut.value << txOut.publicKey;
        return outputStream;
    }

    friend std::istream &operator>>(std::istream &inputStream, TransactionOutput &txOut) {
        inputStream >> txOut.value >> txOut.publicKey;
        return inputStream;
    }
};

struct Transaction {
    typedef std::vector<TransactionOutput>::size_type outputs_size;
    typedef std::vector<TransactionInput>::size_type inputs_size;

    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;

    int64_t hash;

    friend std::ostream &operator<<(std::ostream &outputStream, const Transaction &tx) {
        outputStream << tx.inputs.size();
        for (auto txIn : tx.inputs) {
            outputStream << txIn;
        }
        outputStream << tx.outputs.size();
        for (auto txOut : tx.outputs) {
            outputStream << txOut;
        }
        return outputStream;
    }

    friend std::istream &operator>>(std::istream &inputStream, Transaction &tx) {
        inputs_size inputCount;
        inputStream >> inputCount;
        for (inputs_size i = 0; i < inputCount; ++i) {
            TransactionInput txIn;
            inputStream >> txIn;
            tx.inputs.push_back(txIn);
        }
        outputs_size outputCount;
        inputStream >> outputCount;
        for (outputs_size i = 0; i < outputCount; ++i) {
            TransactionOutput txOut;
            inputStream >> txOut;
            tx.outputs.push_back(txOut);
        }
        return inputStream;
    }
};

#endif /* BLOCKCHAIN_TX_H_ */
