//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package p2p_sim.simulations;

simple POWNode extends P2PNode {
    parameters:
        string defaultNodeList;
        bool newNetwork;
        string minersList;
        string dataDir = default("data");
        bool online = default(true);
        int version = default(1);
        int blocksPerFile = default(10);
        int minAcceptedVersion = default(1);
        int threadScheduleInterval = default(30); // interval at which to check data queues, etc.
        int maxMessageProcess = default(4);  // number of messages to process before passing the execution context
        int maxAddrAd = default(5);  // maximum number of addresses to send in an advertisement
        int numAddrRelay = default(2); // number of peers to relay a new address to
        int addrRelayVecSize = default(10); // limit for relay check
        int dumpAddressesInterval = default(120);  // interval to dump addresses in seconds
        double randomAddressFraction = default(1);  // fraction from 0 to 1
        int blockSyncRecency = default(240); // number of seconds a block for a block to be considered young
        int coinbaseOutput;
        int stopAddrPollingTime;
    @class(POWNode);
    gates:
        input fromScheduler;
}

simple POWScheduler {
    parameters:
        string scheduleFileName;
        int timeToStartSchedule;
        int count;
    @class(POWScheduler);   
    gates:
        output toNodes[count];
}

network POWNetwork {
    parameters:
        string scheduleFileName = default("schedule.txt");
        int count;
        int timeToStartSchedule;
        int stopAddrPollingTime;
        string miners;
        string netDefaultNodeList;
        int coinbaseOutput;  // output in cents of a coinbase transaction (creation of a new coin)
        bool isNewNetwork;
    submodules:
        node[count]: POWNode {
            minersList = miners;
            defaultNodeList = netDefaultNodeList;
            newNetwork = isNewNetwork;
            coinbaseOutput = coinbaseOutput;
            stopAddrPollingTime = stopAddrPollingTime;
        }
        scheduler : POWScheduler {
            scheduleFileName = scheduleFileName;
            count = count;
            timeToStartSchedule = timeToStartSchedule;
        }
    connections:
        for i=0..count-1 {
            scheduler.toNodes[i] --> node[i].fromScheduler;
        }
}
