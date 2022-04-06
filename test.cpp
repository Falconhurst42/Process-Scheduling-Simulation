#include "headers/benchmark.h"
#include <random>
#include <time.h>
#include <vector>

using namespace Simulation;

int main() {
    srand(time(NULL));


    SystemSettings sett;// = SystemSettings::fcfs();
    sett.PROCESS_COUNT = 50;
    sett.RR_TIME = MAX_CPU_BURST + 1;
    std::vector<SystemSettings> setts = cpuRange(sett);
    /*sett.CPU_COUNT = 4;
    sett.RR_TIME = MAX_CPU_BURST + 1;
    setts.push_back(sett);
    sett.RR_TIME = 100;
    setts.push_back(sett);
    sett.RR_TIME = 50;
    setts.push_back(sett);
    sett = SystemSettings::fcfs();
    sett.PROCESS_COUNT = 50;
    sett.CPU_COUNT = 4;
    setts.push_back(sett);*/
    simulateRun(setts.begin(), setts.end(), "RR_201").exportStats();
    //simulate(sett).printStats();
}