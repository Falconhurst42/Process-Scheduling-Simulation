// defines a few type aliases for particular unsigned integral types used in the simulation

#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <vector>
#include <stdint.h>
#include <string>
#include <iostream>

namespace Simulation {
    using Step = uint32_t;
    using PID = uint16_t;
    using Priority = uint8_t;
    using CPUID = std::vector<Step>::size_type;

    const std::string DATA_DIR = "data";
    const Priority MAX_PRIO = 7;
    const unsigned int MAX_BURSTS = 20;
    const Step MAX_CPU_BURST = 200;
    const Step MAX_IO_BURST = 500;
    const Step ARRIVAL_MAX_PER_PROCESS = 50;

    struct SystemSettings {
        CPUID CPU_COUNT = 4;
        PID PROCESS_COUNT = 10;
        Step RR_TIME = 100;
        Step SWITCHING_IN_DELAY = 7;
        Step SWITCHING_OUT_DELAY = 3;

        void print(int indent = 0) const {
            std::string ind(indent, ' ');
            std::cout << ind << "System Settings:" << std::endl;
            std::cout << ind << "    CPUs:          " << CPU_COUNT << std::endl;
            std::cout << ind << "    Processes:     " << PROCESS_COUNT << std::endl;
            std::cout << ind << "    RR Time:       " << RR_TIME << std::endl;
            std::cout << ind << "    Switching In:  " << SWITCHING_IN_DELAY << std::endl;
            std::cout << ind << "    Switching Out: " << SWITCHING_OUT_DELAY << std::endl;
        }

        static SystemSettings fcfs() {
            SystemSettings sett;
            sett.RR_TIME = 0;
            return sett;
        }
    };
    // takes in SystemSettings, returns vector of system settings with varying cpus
    //  increases cpu count logarithmically
    std::vector<SystemSettings> cpuRange(SystemSettings sett, CPUID max = 10, CPUID min = 1) {
        std::vector<SystemSettings> out;
        for(double i = std::log10(min); i <= std::log10(max); i += 0.2) {
            sett.CPU_COUNT = std::pow(10, i);
            if(out.empty() || sett.CPU_COUNT != out.back().CPU_COUNT)
                out.push_back(sett);
        }
        return out;
    }
    std::string to_string(SystemSettings sett) {
        return std::to_string(sett.CPU_COUNT)
         + "_" + std::to_string(sett.PROCESS_COUNT)
          + "_" + std::to_string(sett.RR_TIME)
           + "_" + std::to_string(sett.SWITCHING_IN_DELAY)
            + "_" + std::to_string(sett.SWITCHING_OUT_DELAY);
    }

    enum class ProcessState {ready, running, blocked, exit, switching};
    std::string to_string(ProcessState s) {
        switch(s) {
            case ProcessState::ready:
                return "ready";
            case ProcessState::running:
                return "running";
            case ProcessState::blocked:
                return "blocked";
            case ProcessState::exit:
                return "exit";
            case ProcessState::switching:
                return "switching";
        }
        return "";
    }
    enum class CPUState {idle, assigned_idle, processing, switching_out, switching_in};
    std::string to_string(CPUState s) {
        switch(s) {
            case CPUState::idle:
                return "idle";
            case CPUState::assigned_idle:
                return "assigned_idle";
            case CPUState::processing:
                return "processing";
            case CPUState::switching_out:
                return "switching_out";
            case CPUState::switching_in:
                return "switching_in";
        }
        return "";
    }
}

#endif