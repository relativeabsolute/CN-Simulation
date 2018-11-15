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
        string defaultNodeList; // this will be assigned by the network
        bool online = default(true);
    @class(POWNode);
}

network POWNetwork {
    parameters:
        int count;
        // space-separated list of node indices to connect to
        string defaultNodeList;
        node[*].defaultNodeList = defaultNodeList;
    submodules:
        node[count]: POWNode;
}
