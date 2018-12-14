/*
 * blockchain.cpp
 *
 *  Created on: Dec 9, 2018
 *      Author: jburke
 */

#include "blockchain.h"
#include <boost/filesystem.hpp>
#include <algorithm>

namespace fs = boost::filesystem;

Blockchain::Blockchain(blocks_size blocksPerFile) : blocksPerFile(blocksPerFile) {

}

std::unique_ptr<Blockchain> Blockchain::emptyBlockchain(blocks_size blocksPerFile) {
    Blockchain *tmp = new Blockchain(blocksPerFile);
    return std::unique_ptr<Blockchain>(tmp);
}

std::unique_ptr<Blockchain> Blockchain::readFromDirectory(const std::string &directory) {
    fs::path p(directory);
    if (!fs::exists(p) || !fs::is_directory(p)) {
        return nullptr;
    }
    Blockchain *tmp = new Blockchain(0);
    auto result = std::unique_ptr<Blockchain>(tmp);
    blocks_size numBlocksPerFile = 0;
    blocks_size maxBlocksPerFile = 0;
    for (auto dirEntry : fs::directory_iterator(p)) {
        numBlocksPerFile = result->readBlocksFile(dirEntry.path().string());
        if (numBlocksPerFile > maxBlocksPerFile) {
            maxBlocksPerFile = numBlocksPerFile;
        }
    }
    result->setBlocksPerFile(maxBlocksPerFile);
    // link blocks after they are all read
    for (auto blocksIt = result->blocks.begin(); blocksIt != result->blocks.end(); ++blocksIt) {
        int64_t parentHash = (*blocksIt)->getHeader().parentHash;
        if (parentHash != BlockHeader::NULL_HASH) {
            auto findIt = std::find_if(result->blocks.begin(), result->blocks.end(), [&, parentHash](std::shared_ptr<Block> block) {
                return block->getHeader().hash == parentHash;
            });
            if (findIt != result->blocks.end()) {
                (*blocksIt)->setPrevBlock(*findIt);
            }
        }
    }
    return result;
}

Blockchain::blocks_size Blockchain::readBlocksFile(const std::string &fileName) {
    std::ifstream fileReader(fileName, std::ios::in | std::ios::binary);
    blocks_size numBlocks = 0;
    if (fileReader) {
        fileReader >> numBlocks;
        for (blocks_size i = 0; i < numBlocks; ++i) {
            Block nextBlock;
            fileReader >> nextBlock;
            blocks.push_back(std::make_shared<Block>(nextBlock));
        }
    }
    return numBlocks;
}

void Blockchain::writeToDirectory(const std::string &directory) {
    blocks_size index;
    for (index = 0; index < blocks.size() / blocksPerFile; index += blocksPerFile) {
        std::string fileName = (fs::path(directory) / ("blocks" + std::to_string(index))).string();
        writeBlocksFile(fileName, index, index + blocksPerFile);
    }
}

void Blockchain::writeBlocksFile(const std::string &fileName, int start, int end) {
    std::ofstream fileWriter(fileName, std::ios::in | std::ios::binary);
    if (fileWriter) {
        fileWriter << (end - start);
        for (int i = start; i < end; ++i) {
            fileWriter << blocks.at(i);
        }
    }
}

void Blockchain::addBlock(std::shared_ptr<Block> newBlock) {
    if (newBlock == nullptr || newBlock->getHeader().parentHash != getTip()->getHeader().hash) {
        return;
    }
    blocks.push_back(newBlock);
}

std::shared_ptr<Block> &&Blockchain::findBlockByHash(int64_t hash) {
    auto empty = std::shared_ptr<Block>();
    if (hash == BlockHeader::NULL_HASH) {
        return std::move(empty);
    }
    auto blocksIt = std::find_if(blocks.begin(), blocks.end(), [&, hash](std::shared_ptr<Block> block) {
        return block->getHeader().hash == hash;
    });
    if (blocksIt != blocks.end()) {
        return std::move(*blocksIt);
    } else {
        return std::move(empty);
    }
}

std::vector<std::shared_ptr<Block>> Blockchain::getBlocksAfter(int64_t hash) {
    auto blocksIt = blocks.begin();
    if (hash != BlockHeader::NULL_HASH) {
        blocksIt = std::find_if(blocks.begin(), blocks.end(), [&, hash](std::shared_ptr<Block> block) {
            return block->getHeader().hash == hash;
        });
    }
    std::vector<std::shared_ptr<Block>> result;
    std::copy(blocksIt, blocks.end(), std::back_inserter(result));
    return result;
}

int64_t Blockchain::getMaxTxHash() const {
    if (chainHeight() == 0) {
        return 0;
    }
    int64_t prevMax = 0;
    for (auto tx : getTip()->getTx()) {
        if (tx.hash > prevMax) {
            prevMax = tx.hash;
        }
    }
    return prevMax;
}
