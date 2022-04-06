// defines the variety of types and functions relating to singular processes

#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include "typedefs.h"
#include <list>
#include <iostream>
#include <string>

namespace Simulation {
    class ProcessBursts {
        private:
            std::list<Step> bursts;
            bool processing;
        public:
            template<class iterator_type>
            ProcessBursts(iterator_type first, iterator_type last, bool proc = true) : bursts(first, last), processing(proc) {}
            bool isProcessing() const {
                return processing;
            }
            typename std::list<Step>::const_iterator cbegin() const {
                return bursts.cbegin();
            }
            typename std::list<Step>::const_iterator cend() const {
                return bursts.cend();
            }
            std::list<Step>::size_type size() const {
                return bursts.size();
            }
            // gets the total number of CPU PROCESSING steps remaining
            Step stepsRemaining() const {
                Step total = 0;
                // start at next processing block
                auto it = (processing ? bursts.begin() : ++bursts.begin());
                while(it != bursts.end()) {
                    total += *it;
                    // want to increment by 2
                    // but have to avoid overshooting end of container
                    it++;
                    if(it != bursts.end()) {
                        it++;
                    }
                }
                return total;
            }
            bool empty() const {
                return bursts.empty();
            }
            Step front() const {
                return bursts.front();
            }
            void pop() {
                bursts.pop_front();
                processing = !processing;
            }
            bool step() {
                // NOTE:              VV embedded drecement
                if(!bursts.empty() && --bursts.front() == 0) {
                    pop();
                    return true;
                }
                return false;
            }

            void print() const {
                if(empty()) {
                    std::cout << "[ ]" << std::endl;
                    return;
                }
                std::cout << "[ ";
                bool p = processing;
                for(auto it = bursts.begin(); it != --bursts.end(); it++) {
                    std::cout << (p ? '+' : '-') << *it << ' ';
                    p = !p;
                }
                std::cout << (p ? '+' : '-') << bursts.back() << "]" << std::endl;
            }
    };

    struct ProcessInit {
        PID id;
        Priority prio;
        ProcessBursts bursts;
    };

    struct ProcessPlan {
        Step arrival;
        ProcessInit init;
    };
}

#endif