/*
 * addr_manager.h
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#ifndef ADDR_MANAGER_H_
#define ADDR_MANAGER_H_

#include <vector>

/*! This class is mostly defined for developer convenience, since nodes are represented by integer indices instead of
 * full address info structures.
 *
 */
class AddrManager {
public:
    explicit AddrManager(double fraction);

    std::vector<int> getRandomAddresses();

    void addAddress(int newAddress);
private:
    double addressFraction;
    std::vector<int> addresses;
    size_t numRandomAddresses;
};

#endif /* ADDR_MANAGER_H_ */
