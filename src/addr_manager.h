/*
 * addr_manager.h
 *
 *  Created on: Nov 25, 2018
 *      Author: jburke
 */

#ifndef ADDR_MANAGER_H_
#define ADDR_MANAGER_H_

#include <vector>
#include <set>
#include <memory>

/*! This class is mostly defined for developer convenience, since nodes are represented by integer indices instead of
 * full address info structures.
 *
 */
class AddrManager {
public:
    explicit AddrManager(double fraction);

    /*! Get n random known addresses.  Defaults to the fraction of the currently known addresses
     * defined in the constructor.
     */
    std::vector<int> getRandomAddresses(int n = -1);

    std::set<int> allAddresses() const;

    void addAddress(int newAddress, bool update = true);
    void addAddresses(const std::vector<int> &newAddresses);
private:
    void updateSize();
    double addressFraction;
    std::set<int> addresses;
    size_t numRandomAddresses;
};

#endif /* ADDR_MANAGER_H_ */
