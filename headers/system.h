// defines the CPU and System classes reponsible for the bulk of the simulation

#ifndef SYSTEM_H
#define SYSTEM_H

#include "typedefs.h"
#include "utility.h"
#include "process.h"
#include "stats.h"
#include <cassert>
#include <vector>
#include <queue>
#include <map>
#include <numeric>
#include <algorithm>
#include <limits>
#include <time.h>
#include <sys/stat.h>
#include <cstdlib>
#include <iomanip>
#include <random>


namespace Simulation {
    std::vector<ProcessPlan> generateDataFiles(PID n) {
        std::vector<ProcessPlan> out;
        for(PID i = 0; i < n; i++) {
            // generate random bursts
            std::list<Step> raw_bursts;
            int burst_count = rand() % MAX_BURSTS + 1;
            bool proc_orig = rand() % 2;
            bool proc = proc_orig;
            for(int b = 0; b < burst_count; b++)
                raw_bursts.push_back( rand() % ((proc = !proc) ? MAX_IO_BURST : MAX_CPU_BURST) + 1);
            // generate random prio
            Priority p = rand() % MAX_PRIO;
            ProcessInit init = {i, p, ProcessBursts(raw_bursts.begin(), raw_bursts.end(), proc_orig)};
            // generate random arrival time
            Step arr = rand() % (n*ARRIVAL_MAX_PER_PROCESS) + 1;
            ProcessPlan pl = {arr, init};
            out.push_back(pl);
        }
        return out;
    }

    // a CPU stores
    //      a pointer to the current PCB
    //      a record of the most recent/current PID
    //      a CPUState (in a Timer which indicates how much time is remaining in that state)
    // the CPU is responsible for the following attributes of a PCB
    //      state
    //      steps_used
    //      pcb.bursts
    // the system handles moving a PCB/PID between containers once a CPU is done with it
    class CPU {
        private:
            PCB* proc;
            PID last_id;
            Timer<CPUState> t;  // makes sense to couple state and timer because state change always implies creation of new timer and vice versa
                                // Timers: context_remove(switching_out), context_add(switching_in), round_robin(processing), 0(idle)
            CPUStats stats;
            SystemSettings settings;
        public:
            CPU(SystemSettings sett, CPUID id) : proc(nullptr), last_id(std::numeric_limits<PID>::max()), t(0, CPUState::idle), settings(sett) {
                stats.id = id;
            }

            CPUStats getStats() const {
                return stats;
            }
            CPUState getState() const {
                return t.getData();
            }
            bool isFCFS() const {
                return settings.RR_TIME == 0;
            }
            bool assigned() const {
                return getState() != CPUState::idle;
            }
            // gets the PID of the current process
            // if there is not current process, returns the PID of the last process assigned
            // if this CPU has not yet handled a process, returns the max PID value
            PID getPID() const {
                return last_id;
            }
            // deassigns current process 
            // starts context_remove timer
            void deassign() {
                //std::cout << "deassign() ";
                //std::cout << to_string(proc->state) << std::endl;
                // update process state baed off burst state
                if(proc->bursts.empty()) {
                    proc->state = ProcessState::exit;
                } else {
                    proc->state = ProcessState::switching;
                }
                t = Timer<CPUState>(settings.SWITCHING_OUT_DELAY, CPUState::switching_out);
            }
            // assigns new process 
            // starts context_add timer
            void assign(PCB* p) {
                //std::cout << "assign()" << std::endl;
                //std::cout << to_string(proc->state) << std::endl;
                proc = p;
                last_id = proc->id;
                proc->state = ProcessState::switching;
                t = Timer<CPUState>(settings.SWITCHING_IN_DELAY, CPUState::switching_in);
            }
            // (if not idle) Increments current timer and advances the current process by one step
            // return value of true indicates old process should be returned to ready queue and new process should be assigned
            // returns true when finishing the switching_out timer
            bool step() {
                // get state
                CPUState state = getState();
                stats.hist.inc(state);
                if(!assigned()) {
                    return true;
                } else {
                    // step process and CPU timer
                    bool p_ret = proc->step();
                    bool t_ret = false;
                    // only step if RR or (in case of FCFS) if state != processing/assigned_idle
                    if(!isFCFS() || (state != CPUState::processing && state != CPUState::assigned_idle)) {
                        t_ret = t.step();
                    }

                    // process based off CPU state 
                    switch(state) {
                        case CPUState::assigned_idle:
                        case CPUState::processing:
                            // if process has completed its section
                            if(p_ret || t_ret) {
                                // FCFS has additional checks
                                if(isFCFS()) {
                                    if(proc->bursts.empty()) {
                                        deassign();
                                    } else {
                                        // if not de-assigned, swap state of process and cpu
                                        proc->state = proc->state == ProcessState::running ? ProcessState::blocked : ProcessState::running;
                                        t = proc->state == ProcessState::running ? Timer<CPUState>(0, CPUState::processing) : Timer<CPUState>(0, CPUState::assigned_idle);
                                    }
                                } else {
                                    // RR auto-deassigns
                                    deassign();
                                }
                            }
                            break;
                        case CPUState::switching_out:
                            if(t_ret) {
                                //std::cout << "done switching out" << std::endl;
                                // set the CPU to idle
                                t = Timer<CPUState>(0, CPUState::idle);
                                proc = nullptr;
                                // signal system that the process should be swapped
                                return true;
                            }
                            break;
                        case CPUState::switching_in:
                            if(t_ret) {
                                //std::cout << "done switching in" << std::endl;
                                if(isFCFS()) {
                                    // state = running if RR or (in case of FCFS) if isProcessing()
                                    // else (FCFS IO) state = blocked
                                    proc->state = proc->bursts.isProcessing() ? ProcessState::running : ProcessState::blocked;
                                    t = proc->state == ProcessState::running ? Timer<CPUState>(0, CPUState::processing) : Timer<CPUState>(0, CPUState::assigned_idle);
                                } else {
                                    // start RR timer and indicate that CPU and process are now processing
                                    proc->state = ProcessState::running;
                                    t = Timer<CPUState>(settings.RR_TIME, CPUState::processing);
                                }
                            }
                            break;
                    }
                }
                return false;
            }
    };

    class System {
        private:
            SystemSettings settings;
            std::vector<CPU> cpus;
            std::map<PID, PCB> PCB_table;
            std::list<PCB> retired;
            ReadyPriorityQueue<PID> ready;
            std::list<PID> blocked;      // can't use actual queue cuz this isn't actually FIFO
            std::list<Timer<ProcessInit>> process_entry_timers;

            void addProcess(const ProcessInit& pi, Step curr) {
                // add to table
                PCB_table.emplace(pi.id, PCB(pi, curr));
                // add to appropriate queue
                if(pi.bursts.isProcessing()) {
                    ready.push(pi.id, pi.prio);
                } else {
                    PCB_table.at(pi.id).state = ProcessState::blocked;
                    blocked.push_back(pi.id);
                }
            }

            void clearState() {
                PCB_table.clear();
                retired.clear();
                ready.clear();
                blocked.clear();
                process_entry_timers.clear();
                cpus.clear();
            }

        public:
            void updateSettings(SystemSettings sett) {
                clearState();
                settings = sett;
                while(cpus.size() < settings.CPU_COUNT)
                    cpus.emplace_back(settings, cpus.size());
            }

            System(SystemSettings sett = SystemSettings()) {
                updateSettings(sett);
            }

            // BIG DADDY
            void simulate(std::vector<ProcessPlan> data_files) {
                // for each ProcessPlan, start ProcessEntry timer
                for(auto& plan : data_files)
                    process_entry_timers.emplace_back(plan.arrival, plan.init);
                
                // Simulate steps until max reached or all processes finish
                for(Step s = 0; s < std::numeric_limits<Step>::max() && !(PCB_table.empty() && process_entry_timers.empty()); s++) {
                    // for each CPU
                    for(auto &cpu : cpus) {
                        // save if CPU was already idle (have to save this cuz step() will change state to idle when it returns true)
                        bool already_idle = !cpu.assigned();
                        // If not already idle, step and check if needs a new process
                        if(cpu.step() || already_idle) {
                            // don't move CPU's last process if it was already idle
                            // already idle CPUs have either never had a process, or already had their previous process handled on a previous loop
                            if(!already_idle) {
                                // move CPU's last process (specific action depends on state)
                                PID id = cpu.getPID();
                                PCB pcb = PCB_table.at(id);
                                //printPCB(pcb, 4);
                                if(pcb.state == ProcessState::exit) {
                                    //std::cout << "Deleting: " << id << std::endl;
                                    // delete PCB (and save to retired vector)
                                    auto it = PCB_table.find(id);
                                    retired.push_back((*it).second);
                                    PCB_table.erase(it);
                                } else if(pcb.bursts.isProcessing()) {
                                    //std::cout << "Adding to ready: " << id << std::endl;
                                    // add to ready RPQ
                                    ready.push(id, pcb.prio);
                                    PCB_table.at(id).state = ProcessState::ready;
                                } else {
                                    //std::cout << "Adding to blocked: " << id << std::endl;
                                    // add to blocked list
                                    blocked.push_back(id);
                                    PCB_table.at(id).state = ProcessState::blocked;
                                }
                                //printPCB(pcb);
                            }

                            // check if there's a process available
                            if(!ready.empty()) {
                                // assign process
                                //std::cout << "Assigning " << ready.front() << " to CPU " << cpun << std::endl;

                                cpu.assign(&(PCB_table.at(ready.front())));
                                // remove process from ready queue
                                ready.pop();
                            }
                        }
                    }

                    // step all blocked processes
                    for(auto it = blocked.begin(); it != blocked.end(); ) {
                        // get iterator to pcb
                        PID id = *it;
                        auto pcb_it = PCB_table.find(id);
                        // step and check
                        if((*pcb_it).second.step()) {
                            // check for being completely finished
                            if((*pcb_it).second.bursts.empty()) {
                                // delete from PCB table and save to retired
                                retired.push_back((*pcb_it).second);
                                PCB_table.erase(pcb_it);
                            } else {
                                // add to ready list
                                ready.push(id, (*pcb_it).second.prio);
                                // update state to ready
                                (*pcb_it).second.state = ProcessState::ready;
                            }
                            // remove from blocked list (and get new iterator)
                            it = blocked.erase(it);
                        } else {
                            // only increment if not removed from blocked list
                            it++;
                        }
                    }

                    // step all ready processes
                    for(auto id : ready)
                        PCB_table.at(id).step();
                    
                    // step ProcessEntry Timers
                    for(auto it = process_entry_timers.begin(); it != process_entry_timers.end(); ) {
                        // step and check
                        if((*it).step()) {
                            // create Process
                            addProcess((*it).getData(), s);
                            // delete Timer (and get new iterator)
                            it = process_entry_timers.erase(it);
                        } else {
                            // only increment if Timer is not deleted
                            it++;
                        }
                    }
                }
            }

            // simulate with current settings
            void simulate() {
                simulate(generateDataFiles(settings.PROCESS_COUNT));
            }

            SimulationStats outputStats() {
                std::vector<ProcessStats> ps;
                std::vector<CPUStats> cs;
                for(auto& p : retired)
                    ps.push_back(p.stats);
                for(auto& c : cpus)
                    cs.push_back(c.getStats());
                return SimulationStats(settings, ps.begin(), ps.end(), cs.begin(), cs.end());
            }
    };
}

#endif