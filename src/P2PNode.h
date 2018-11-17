/*
 * P2PNode.h
 *
 *  Created on: Nov 14, 2018
 *      Author: jburke
 */

#ifndef P2PNODE_H_
#define P2PNODE_H_

#include <omnetpp.h>

using namespace omnetpp;

/*! \mainpage Network simulation documentation
 *
 * \section intro_sec Introduction
 *
 * This is the documentation for the simulation of blockchain-based network applications.  The simulation provides
 * Proof of Work, Proof of Stake, and Proof of Reliability networks and various exploits of security vulnerabilities in these networks, as well as parameter
 * configuration to address these security vulnerabilities.
 *
 * See https://github.com/relativeabsolute/CN-Simulation for further information.
 *
 * \section Navigation
 * Use the buttons <code>Code</code> or <code>Files</code> at the top of the page to start navigating the code.
 *
 */

/*! Base class for peer to peer nodes.  Has no functionality, is simply defined for the basic peer to peer simulation.
 *
 */
class P2PNode : public cSimpleModule {
};

Define_Module(P2PNode)

#endif /* P2PNODE_H_ */
