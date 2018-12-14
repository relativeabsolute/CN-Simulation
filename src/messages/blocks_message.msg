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

cplusplus {{
    #include "../blockchain/block.h"
    #include "pow_message_m.h"
    #include <vector>
    #include <memory>
    typedef std::vector<Block> blocksVector;
}};

message POWMessage;
struct Block;
class noncobject blocksVector;

message BlocksMessage extends POWMessage {
    blocksVector blocks;
}