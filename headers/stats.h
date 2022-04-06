// defines

#ifndef STATS_H
#define STATS_H

#include "typedefs.h"
#include "process_utils.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <string>
#include <sstream>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <stdint.h>

namespace Simulation {
    template <typename E>
    class History {
        private:
            struct Period {
                E state;
                Step duration;

                Period& operator+=(Step d) {
                    duration += d;
                    return *this;
                }
            };

            std::vector<Period> trace;
        public:
            History() {}

            std::vector<Period> getTrace() const {
                return trace;
            }
            typename std::vector<Period>::const_iterator cbegin() const {
                return trace.cbegin();
            }
            typename std::vector<Period>::const_iterator cend() const {
                return trace.cend();
            }
            typename std::vector<Period>::const_iterator begin() const {
                return cbegin();
            }
            typename std::vector<Period>::const_iterator end() const {
                return cend();
            }

            void push(E state, Step duration) {
                if(trace.empty() || trace.back().state != state) {
                    Period p = {state, duration};
                    trace.push_back(p);
                } else {
                    trace.back() += duration;
                }
            }

            void inc(E state) {
                push(state, 1);
            }

            /**/Step duration() const {
                return std::accumulate(
                    trace.cbegin(), trace.cend(), /**/Step(0), 
                    [](/**/Step sum, Period p) {
                        return sum + p.duration;
                    }
                );
            }

            // get duration of particular state
            /**/Step duration(E state) const {
                return std::accumulate(
                    trace.cbegin(), trace.cend(), /**/Step(0), 
                    [state](/**/Step sum, Period p) {
                        return p.state == state ? sum + p.duration : sum;
                    }
                );
            }

            void print(int indent = 0) const {
                std::string ind(indent, ' ');
                for(auto& p : trace)
                    std::cout << ind << to_string(p.state) << ": " << p.duration << std::endl;
            }

            void printPercentages(int indent = 0) const {
                // pre-gen constants
                std::string ind(indent, ' ');
                double d = duration();
                // get max length of enum strings
                int max = 0;
                for(auto& p : trace)
                    max = std::max(max, (int)(to_string(p.state).length()));
                // output
                for(auto& p : trace)
                    std::cout << std::setprecision(5) << ind << to_string(p.state) << ": " << std::string(max - to_string(p.state).length(), ' ') << 100 * p.duration / d << "%" << std::endl;
            }

            std::string to_timeline_csv() const {
                std::ostringstream out_s;
                out_s << "state,duration" << std::endl;
                for(auto& t : trace)
                    out_s << to_string(t.state) << "," << t.duration << std::endl;

                return out_s.str();
            }

            std::string to_piechart_csv() const {
                // output to string
                std::ostringstream out_s;
                out_s << "state,duration" << std::endl;
                for(auto& s : collapseSums(*this))
                    out_s << to_string(s.state) << "," << s.duration << std::endl;

                return out_s.str();
            }
    };

    // collapse a set of Historys into sums of states
    // final argument is a hint for the templating deduction
    template<typename E, typename iterator_type>
    History<E> collapseSums(iterator_type start, iterator_type end, E) {
        // calculate sums
        std::map<E, Step> sums;
        while(start != end)
            for(auto& t : (*(start++)).getTrace())
                sums[t.state] += t.duration;
        
        History<E> out;
        for(auto& s : sums)
            out.push(s.first, s.second);
        return out;
    }

    template<typename E>
    History<E> collapseSums(History<E> source) {
        if(source.begin() != source.end())
            return collapseSums(&source, &source + 1, source.getTrace().front().state);
        return History<E>();
    }

    // stats tracked per Process
    struct ProcessStats {
        const PID id;
        const Priority prio;
        Step started;
        ProcessBursts plan;
        History<ProcessState> hist;

        ProcessStats(const ProcessInit& pi, Step s, History<ProcessState> h) : id(pi.id), prio(pi.prio), started(s), plan(pi.bursts), hist(h) {}

        // Total time in history (ready + processing + blocked)
        Step getTurnaround() const {
            return hist.duration();
        }

        // Total time waited
        Step getWait() const {
            return hist.duration(ProcessState::ready) + hist.duration(ProcessState::switching) + hist.duration(ProcessState::exit);
        }

        // Max time between IO
        Step getResponse() const {
            Step max = 0;
            Step cur = 0;
            auto end = hist.cend();
            for(auto it = hist.cbegin(); it != end; it++) {
                if((*it).state == ProcessState::blocked) {
                    max = std::max(max, cur);
                    cur = 0;
                } else {
                    cur += (*it).duration;
                }
            }
            return max;
        }
        // (Max time between IO) / (Max CPUBurst)
        // adjusts Response stat for hard-coded long processing
        double getResponseAdjusted() const {
            Step resp = getResponse();
            // can't adjust perfect response
            if(resp == 0) 
                return 0;
            
            bool p = plan.isProcessing();
            Step max_burst = 0; 
            for(auto it = plan.cbegin(); it != plan.cend(); it++) {
                // toggle processing and only go for processing
                if(!(p = !p)) {
                    max_burst = std::max(max_burst, *it);
                }
            }
            return (double)(resp) / (double)(max_burst);
        }

        void print(int indent = 0) const {
            std::string ind(indent, ' ');
            std::cout << ind << "PCB " << id << std::endl;
            std::cout << ind << "    priority: " << (int)(prio) << std::endl;
            std::cout << ind << "    turnaround: " << getTurnaround() << std::endl;
            std::cout << ind << "    wait: " << getWait() << std::endl;
            std::cout << ind << "    response: " << getResponse() << std::endl;
            std::cout << ind << "       (adjusted): " << std::setprecision(5) << getResponseAdjusted() << std::endl;
            std::cout << ind << "    started: " << started << std::endl;
            std::cout << ind << "    hist: " << std::endl;   hist.print(indent+8);
        }
    };

    // stats tracked per CPU
    struct CPUStats {
        CPUID id;
        History<CPUState> hist;

        double getStatePercent(CPUState state) const {
            return hist.duration(state) / (double)(hist.duration());
        }

        void print(int indent = 0) const {
            std::string ind(indent, ' ');
            std::cout << ind << "CPU " << id << std::setprecision(5) << std::endl;
            std::cout << ind << "    Processing:    " << 100*getStatePercent(CPUState::processing) << "%" << std::endl;
            std::cout << ind << "    Assigned Idle: " << 100*getStatePercent(CPUState::assigned_idle) << "%" << std::endl;
            std::cout << ind << "    Idle:          " << 100*getStatePercent(CPUState::idle) << "%" << std::endl;
            std::cout << ind << "    Switching In:  " << 100*getStatePercent(CPUState::switching_in) << "%" << std::endl;
            std::cout << ind << "    Switching Out: " << 100*getStatePercent(CPUState::switching_out) << "%" << std::endl;
        }
    };

    // all stats for a single simulation
    struct SimulationStats {
        SystemSettings settings;
        std::vector<ProcessStats> ps;
        std::vector<CPUStats> cs;

        template<class PSIt, class CSIt>
        SimulationStats(SystemSettings sett, PSIt p_start, PSIt p_end, CSIt c_start, CSIt c_end) : settings(sett), ps(p_start, p_end), cs(c_start, c_end) {}

        // units: Proc / Step
        double getThroughput() const {
            /**/Step total_steps = cs.front().hist.duration();
            return settings.PROCESS_COUNT / (double)(total_steps);
        }
        double getAvgTurnaround() const {
            /**/Step sum = 0;
            for(auto& p : ps)
                sum += p.getTurnaround();
            
            return sum / (double)(ps.size());
        }
        double getAvgWait() const {
            /**/Step sum = 0;
            for(auto& p : ps)
                sum += p.getWait();
            
            return sum / (double)(ps.size());
        }
        double getAvgResponse() const {
            /**/Step sum = 0;
            for(auto& p : ps)
                sum += p.getResponse();
            
            return sum / (double)(ps.size());
        }
        double getAvgResponseAdjusted() const {
            double sum = 0;
            for(auto& p : ps)
                sum += p.getResponseAdjusted();
            
            return sum / (double)(ps.size());
        }
        // divide by number of CPUs
        double adjustForCPUs(double n) const {
            return n / cs.size();
        }
        // multiply by average process length
        double getAvgProcessLength() const {
            /**/Step total_process_length = 0;
            for(auto& p : ps)
                for(auto it = p.plan.cbegin(); it != p.plan.cend(); it++)
                    total_process_length += *it;
            
            return total_process_length / (double)(settings.PROCESS_COUNT);
        }

        History<CPUState> collapseCPUHistory() const {
            std::list<History<CPUState>> hists;
            std::for_each(cs.begin(), cs.end(), [&hists](const CPUStats& c){ hists.push_back(c.hist); });
            return collapseSums(hists.begin(), hists.end(), CPUState::idle);
        }
        History<ProcessState> collapseProcessHistory() const {
            std::list<History<ProcessState>> hists;
            std::for_each(ps.begin(), ps.end(), [&hists](const ProcessStats& p){ hists.push_back(p.hist); });
            return collapseSums(hists.begin(), hists.end(), ProcessState::exit);
        }
        void printCPUStatsSummary() const {
            std::cout << std::endl << "CPU Stats: " << std::endl;
            collapseCPUHistory().printPercentages(4);
        }
        void printProcessStatsSummary() const {
            std::cout << std::endl << "Process Stats: " << std::endl;
            collapseProcessHistory().printPercentages(4);
        }
        void printCPUStatsFull() const {
            std::cout << std::endl << "CPU Stats: " << std::endl;
            for(auto& c : cs) {
                c.print(4);
            }
        }
        void printPCBStatsFull() const {
            std::cout << std::endl << "PCB Stats: " << std::endl;
            for(auto& p : ps) {
                p.print(4);
            }
        }

        void printStats() const {
            settings.print();

            std::cout << std::setprecision(5) << std::endl << "Overall Stats: " << std::endl;
            std::cout << "    Avg Process Length: " << getAvgProcessLength() << " Steps" << std::endl;
            std::cout << "    Avg Turnaround:     " << getAvgTurnaround() << " Steps" << std::endl;
            std::cout << "    Avg Wait:           " << getAvgWait() << " Steps" << std::endl;
            std::cout << "    Avg Response Time: " << std::endl;
            double stat = getAvgResponse();
            std::cout << "        Raw:            " << stat << " Steps" << std::endl;
            stat = getAvgResponseAdjusted();
            std::cout << "        Adjusted:       " << stat << "x longer" << std::endl;
            std::cout << "    Throughput: " << std::endl;
            stat = getThroughput();
            std::cout << "        Raw:            "<< stat << " Proc per Step           (" << 1/stat << " Steps per Proc)" << std::endl;
            stat = adjustForCPUs(stat);
            std::cout << "        CPU Adjusted:   " << stat << " Proc per Step per CPU  (" << 1/stat << " Step per Proc per CPU)" << std::endl;

            std::cout << std::endl;
            printCPUStatsSummary();
            std::cout << std::endl;
            printProcessStatsSummary();
            std::cout << std::endl;
        }

        // generate unique folder name with some settings info
        // CPUCOUNT_PROCESSCOUNT_RRTIME_XXX
        std::string getFolderName() const {
            return DATA_DIR + "/"
             + to_string(settings)
              + "_" + std::to_string(time(NULL) % 1000);
        }

        void exportStats(std::string folder) const {
            // output each ProcessStats to .csv
            std::string dir = folder + "/timelines/processes/inputs";
            std::string cmd = "mkdir -p " + dir;
            system(cmd.c_str());
            
            std::string path;
            int i = 0;
            for(auto& p : ps) {
                path = dir + "/" + std::to_string(i++) + ".csv";
                std::ofstream f(path, std::ofstream::out);
                if(!f.is_open())
                    throw "Error opening file " + path;
                
                f << p.hist.to_timeline_csv();
            }
            
            // output each CPUStats to .csv
            dir = folder + "/timelines/cpus/inputs";
            cmd = "mkdir -p " + dir;
            system(cmd.c_str());
            
            i = 0;
            for(auto& c : cs) {
                path = dir + "/" + std::to_string(i++) + ".csv";
                std::ofstream f(path, std::ofstream::out);
                if(!f.is_open())
                    throw "Error opening file " + path;
                
                f << c.hist.to_timeline_csv();
            }

            // output each ProcessStats to .csv
            dir = folder + "/piecharts/processes/inputs";
            cmd = "mkdir -p " + dir;
            system(cmd.c_str());
            
            i = 0;
            for(auto& p : ps) {
                path = dir + "/" + std::to_string(i++) + ".csv";
                std::ofstream f(path, std::ofstream::out);
                if(!f.is_open())
                    throw "Error opening file " + path;
                
                f << p.hist.to_piechart_csv();
            }
            
            // output each CPUStats to .csv
            dir = folder + "/piecharts/cpus/inputs";
            cmd = "mkdir -p " + dir;
            system(cmd.c_str());
            
            i = 0;
            for(auto& c : cs) {
                path = dir + "/" + std::to_string(i++) + ".csv";
                std::ofstream f(path, std::ofstream::out);
                if(!f.is_open())
                    throw "Error opening file " + path;
                
                f << c.hist.to_piechart_csv();
            }
            
            // make avg pi charts
            if(cs.size() > 1) {
                path = folder + "/piecharts/cpus/inputs/avg.csv";
                std::ofstream cpu_avg_pi(path, std::ofstream::out);
                if(!cpu_avg_pi.is_open())
                    throw "Error opening file " + path;
                cpu_avg_pi << collapseCPUHistory().to_piechart_csv();
            }
            if(ps.size() > 1) {
                path = folder + "/piecharts/processes/inputs/avg.csv";
                std::ofstream proc_avg_pi(path, std::ofstream::out);
                if(!proc_avg_pi.is_open())
                    throw "Error opening file " + path;
                proc_avg_pi << collapseProcessHistory().to_piechart_csv();
            }
        }

        void exportStats() const {
            exportStats(getFolderName());
        }

        static std::string to_csv_header() {
            return "Settings,Process Length,Turnaround,Wait,Response,Response Adjusted,Throughput,Throughput INV,Throughput CPU,CPU Processing%";
        }

        // Settings,Turnaround,Wait,Response,Throughput,Throughput INV,Throughput CPU,CPU Avg,CPU Max,CPU Min
        std::string to_csv_row() const {
            std::ostringstream out;

            out << to_string(settings) << "," << std::setprecision(5)
                << getAvgProcessLength() << ","
                << getAvgTurnaround() << ","
                << getAvgWait() << ","
                << getAvgResponse() << ","
                << getAvgResponseAdjusted() << ","
                << getThroughput() << ","
                << 1/getThroughput() << ","
                << adjustForCPUs(1/getThroughput()) << ","
                << 100 * collapseCPUHistory().duration(CPUState::processing) / (double)(collapseCPUHistory().duration());

            return out.str();
        }
    };

    // stats for multiple simulations of a given setup
    struct ManyStats {
        std::string name;
        std::vector<SimulationStats> runs;

        // generate unique folder name with some settings info
        // ...
        std::string getFolderName() const {
            return DATA_DIR + "/" + name;
        }

        void exportStats() const {
            std::string folder = getFolderName();
            for(auto& run : runs)
                run.exportStats(folder + "/" + to_string(run.settings));

            // compile into summary.csv
            std::ofstream summ(folder + "/" + "summary.csv", std::ofstream::out);
            if(!summ.is_open())
                throw "Error opening file" + folder + "/" + "summary.csv";
            summ << SimulationStats::to_csv_header() << std::endl;
            for(auto& run : runs)
                summ << run.to_csv_row() << std::endl;            
        }
    };
}

#endif