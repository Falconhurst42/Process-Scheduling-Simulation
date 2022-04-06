// defines a few type aliases for particular unsigned integral types used in the simulation

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <random>
#include <map>
#include <vector>
#include <math.h>
#include <time.h>
#include "typedefs.h"
#include "system.h"
#include "stats.h"

namespace Simulation {
    SimulationStats simulate(SystemSettings sett, std::vector<ProcessPlan> data_files) {
        System sys(sett);
        sys.simulate(data_files);
        return sys.outputStats();
    }

    SimulationStats simulate(SystemSettings sett) {
        return simulate(sett, generateDataFiles(sett.PROCESS_COUNT));
    }

    SimulationStats simulate() {
        return simulate(SystemSettings());
    }

    // simulate many runs
    // for each unique number of processes, use the same process plan
    template<class iterator_type>
    ManyStats simulateRun(iterator_type start, iterator_type end, std::string name = "") {
        ManyStats stats;
        std::map<PID, std::vector<ProcessPlan>> plan_map;
        while(start != end) {
            PID n = (*start).PROCESS_COUNT;
            // add to map if not already there
            if(plan_map.find(n) == plan_map.end())
                plan_map[n] = generateDataFiles(n);
            stats.runs.push_back(simulate(*(start++), plan_map[n]));
        }
        if(name == "")
            name = std::to_string(time(NULL));
        stats.name = name;
        return stats;
    }

    // Runs simulations for the given Systemsettings with a different number of CPUs
    // the results are printed and exported to the data folder specified by "name"
    // NOTE: CPU count varies logarithmically, not linearly
    void testCPURange(SystemSettings sett, std::string name, CPUID max = 10, CPUID min = 1) {
        auto setts = cpuRange(sett);
        ManyStats stats = simulateRun(setts.begin(), setts.end(), name);

        // throughput comparison by CPU count
        CPUID min_count = stats.runs.front().settings.CPU_COUNT;
        std::map<CPUID, Step> thru;
        for(auto& run : stats.runs) {
            run.printStats();
            thru[run.settings.CPU_COUNT] = 1/run.getThroughput();
            min_count = std::min(min_count, run.settings.CPU_COUNT);
        }
        std::cout << "CPU Throughput Comparison:" << std::endl;
        std::cout << "    " << min_count << " CPU: " << thru[min_count] << " Steps/Process" << std::setprecision(2) << std::endl;
        for(auto& pair : thru)
            if(pair.first != min_count)
                std::cout << "    " << pair.first << " CPU: " << thru[min_count]/(double)(pair.second) << "x faster" << std::endl;

        stats.exportStats();
    }
}

#endif