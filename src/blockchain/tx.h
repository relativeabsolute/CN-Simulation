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

struct Transaction {
    typedef std::map<int, double>::size_type outputs_size;

    int input;
    std::map<int, double> outputs;

    friend std::ostream &operator<<(std::ostream &outputStream, const Transaction &tx) {
        outputStream << outputs.size() << input;
        for (auto pair : outputs) {
            outputStream << pair.first << pair.second;
        }
    }

    friend std::istream &operator>>(std::istream &inputStream, Transaction &tx) {
        outputs_size count;
        inputStream >> count >> tx.input;
        for (outputs_size i = 0; i < count; ++i) {
            int outputDest;
            double outputAmount;
            inputStream >> outputDest >> outputAmount;
            tx.outputs.insert(std::make_pair<int, double>(outputDest, outputAmount));
        }
    }
};

#endif /* BLOCKCHAIN_TX_H_ */
