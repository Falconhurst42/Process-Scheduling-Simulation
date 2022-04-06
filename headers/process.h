// defines the PCB class which represents a process
// this was split from the file "process_utils.h" to avoid a circular dependency with "stats.h"

#ifndef PROCESS_H
#define PROCESS_H

#include "stats.h"
#include "typedefs.h"
#include "process_utils.h"
#include <iostream>
#include <iomanip>
#include <string>

namespace Simulation {
    struct PCB {
        const PID id;
        ProcessState state;         // handled by CPU (and System, in case of unblocking)
        const Priority prio;
        ProcessStats stats;
        ProcessBursts bursts;       // handled by CPU

        PCB(const ProcessInit& pi, Step curr) : 
            id(pi.id), 
            state(ProcessState::ready),
            prio(pi.prio),
            bursts(pi.bursts),
            stats(pi, curr, History<ProcessState>()) {}
        
        bool step() {
            // this function may be called in the blocked/exit state (when the context is switching out)
            // but that time is not counted as "waited" because the process is not ready yet
            // for these purposes, "waiting" implies wasting clocks cycles while ready
            stats.hist.inc(state);
            if(state == ProcessState::running || state == ProcessState::blocked)
                return bursts.step();
            return false;
        }
    };

    void printPCB(const PCB& pcb, int indent = 0) {
        std::string ind(indent, ' ');
        std::cout << ind << "PCB id: " << pcb.id << std::endl;
        std::cout << ind << "    state: " << to_string(pcb.state) << std::endl;
        std::cout << ind << "    bursts: ";    pcb.bursts.print();
    }
}


#endif