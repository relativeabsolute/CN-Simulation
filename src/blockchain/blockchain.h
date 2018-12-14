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
    typedef std::vector<std::shared_ptr<Block>>::size_type blocks_size;
    /*! Read a blockchain structure from the given directory.  Blocks are written to files in a segmented fashion (so as not to
     * create one huge file).  Each file contains a header that indicates number of blocks, followed by that number of blocks.
     * \param directory Directory containing segmented block files.
     * \returns Pointer to resulting blockchain structure.
     */
    static std::unique_ptr<Blockchain> readFromDirectory(const std::string &directory);
    static std::unique_ptr<Blockchain> emptyBlockchain(blocks_size blocksPerFile);
    void writeToDirectory(const std::string &directory);

    void addBlock(std::shared_ptr<Block> newBlock);

    void setBlocksPerFile(blocks_size blocksPerFile) {
        this->blocksPerFile = blocksPerFile;
    }

    blocks_size getBlocksPerFile() const {
        return blocksPerFile;
    }

    std::shared_ptr<Block> &&findBlockByHash(int64_t hash);

    int64_t getMaxTxHash() const;

    /*! Get the blocks after and including the block with the given hash
     *
     */
    std::vector<std::shared_ptr<Block>> getBlocksAfter(int64_t hash);

    std::shared_ptr<Block> &getTip() {
        return blocks[blocks.size() - 1];
    }

    const std::shared_ptr<Block> &getTip() const {
        return blocks[blocks.size() - 1];
    }

    size_t chainHeight() const {
        return blocks.size();
    }

private:
    blocks_size readBlocksFile(const std::string &fileName);
    void writeBlocksFile(const std::string &fileName, int start, int end);

    explicit Blockchain(blocks_size blocksPerFile);
    std::vector<std::shared_ptr<Block>> blocks;
    blocks_size blocksPerFile;
};

#endif /* BLOCKCHAIN_BLOCKCHAIN_H_ */
