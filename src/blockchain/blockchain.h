/*
 * blockchain.h
 *
 *  Created on: Dec 9, 2018
 *      Author: jburke
 */

#ifndef BLOCKCHAIN_BLOCKCHAIN_H_
#define BLOCKCHAIN_BLOCKCHAIN_H_

#include "block.h"
#include <list>

class Blockchain {
public:
    /*! Read a blockchain structure from the given directory.  Blocks are written to files in a segmented fashion (so as not to
     * create one huge file).  Each file contains a header that indicates number of blocks, followed by that number of blocks.
     * \param directory Directory containing segmented block files.
     * \returns Pointer to resulting blockchain structure.
     */
    static std::unique_ptr<Blockchain> readFromDirectory(const std::string &directory);
private:
    typedef std::vector<Block>::size_type blocks_size;

    void readBlocksFile(const std::string &fileName);

    Blockchain();
    std::vector<Block> blocks;
};

#endif /* BLOCKCHAIN_BLOCKCHAIN_H_ */
