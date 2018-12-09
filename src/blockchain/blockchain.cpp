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

std::unique_ptr<Blockchain> Blockchain::readFromFile(const std::string &directory) {
    fs::path p(directory);
    if (!fs::exists(p) || !fs::is_directory(p)) {
        return nullptr;
    }
    auto result = std::make_unique<Blockchain>();
    for (auto dirEntry : fs::directory_iterator(p)) {
        result->readBlocksFile(dirEntry.path().string());
    }
    // build structure after everything is read
    std::sort(blocks.begin(), blocks.end(), [](const Block& first, const Block& second) {
        return first.getHeader()->parentHash < second.getHeader()->parentHash;
    });
}

void Blockchain::readBlocksFile(const std::string &fileName) {
    std::ifstream fileReader(fileName, std::ios::in | std::ios::binary);
    if (fileReader) {
        blocks_size numBlocks;
        fileReader >> numBlocks;
        for (blocks_size i = 0; i < numBlocks; ++i) {
            Block nextBlock;
            fileReader >> nextBlock;
            blocks.push_back(nextBlock);
        }
    }
}
