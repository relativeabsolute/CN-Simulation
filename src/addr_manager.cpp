/*
 * addr_manager.cpp
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#include "addr_manager.h"
#include <algorithm>
#include <cmath>
#include <omnetpp.h>

std::vector<int> AddrManager::getRandomAddresses(int n) {
    // algorithm: shuffle addresses, then return subvector of length numRandomAddresses
    if (n == -1) {
        n = numRandomAddresses;
    }

    std::vector<int> result;
    std::copy(addresses.begin(), addresses.end(), std::back_inserter(result));
    std::random_shuffle(result.begin(), result.end());

    return std::vector<int>(result.begin(), result.begin() + n);
}

std::set<int> AddrManager::allAddresses() const {
    return addresses;
}

AddrManager::AddrManager(double fraction) : addressFraction(fraction) {
    updateSize();
}

void AddrManager::addAddress(int newAddress, bool update) {
    addresses.insert(newAddress);
    if (update) {
        updateSize();
    }
}

void AddrManager::addAddresses(const std::vector<int> &newAddresses) {
    for (int address : newAddresses) {
        addAddress(address, false);
    }
    updateSize();
}

void AddrManager::updateSize() {
    numRandomAddresses = (size_t)ceil(addresses.size() * addressFraction);
}
