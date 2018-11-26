/*
 * addr_manager.cpp
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#include "addr_manager.h"
#include <algorithm>
#include <cmath>

std::vector<int> AddrManager::getRandomAddresses() {
    // algorithm: shuffle addresses, then return subvector of length numRandomAddresses
    std::random_shuffle(addresses.begin(), addresses.end());

    return std::vector<int>(addresses.begin(), addresses.begin() + numRandomAddresses);
}

AddrManager::AddrManager(double fraction) : addressFraction(fraction) {
    numRandomAddresses = (size_t)ceil(addresses.size() * fraction);
}

void AddrManager::addAddress(int newAddress) {
    addresses.push_back(newAddress);
}
