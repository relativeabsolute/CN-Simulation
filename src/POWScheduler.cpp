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

#include "POWScheduler.h"
#include <iostream>
#include <fstream>
#include <string>
#include "messages/scheduler_message_m.h"

POWScheduler::POWScheduler() {

}

POWScheduler::~POWScheduler() {

}

void POWScheduler::initialize() {
    int timeToStartSchedule = par("timeToStartSchedule").intValue();
    simtime_t time = simTime() + timeToStartSchedule;
    EV << "Schedule to start at " << time;
    scheduleAt(time, new cMessage("start_schedule"));
}

void POWScheduler::handleMessage(cMessage *msg) {
    std::string msgName = msg->getName();
    if (msgName == "start_schedule") {
        bubble("Starting schedule");
        EV << "Starting schedule." << std::endl;
        std::ifstream fileReader(par("scheduleFileName").stringValue());
        if (fileReader) {
            for (std::string line; std::getline(fileReader, line);) {
                // schedule syntax: timeToSend nodeAddress messageType messageParams
                // messageParams is value,value,value
                // each schedule message has its expected order and number of parameters
                cStringTokenizer tok(line.c_str(), " ");
                int time = std::stoi(tok.nextToken());
                int address = std::stoi(tok.nextToken());
                std::string messageType = tok.nextToken();
                std::vector<int> parameters;
                if (tok.hasMoreTokens()) {
                    parameters = cStringTokenizer(tok.nextToken(), ",").asIntVector();
                }
                auto msg = new SchedulerMessage(std::move(messageType).c_str());
                msg->setParameters(parameters);
                EV << "Scheduling message to be sent to " << address
                        << " in " << time << "s" << std::endl;
                sendDelayed(msg, simTime() + time, "toNodes", address);
            }
        }
    }
    delete msg;
}
