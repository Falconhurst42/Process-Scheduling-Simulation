// Templated container classes ReadyPriorityQueue and Timer

#ifndef UTILITY_H
#define UTILITY_H

#include "typedefs.h"
#include <cassert>
#include <vector>
#include <queue>

namespace Simulation {
    // lower Priority value ===> higher priority
    template<typename T>
    class ReadyPriorityQueue {
        public:
            class const_iterator {
                using iterator_category = std::forward_iterator_tag;
                using difference_type   = std::ptrdiff_t;
                using value_type        = T;
                using pointer           = const T*;  // or also value_type*
                using reference         = const T&;  // or also value_type&

                private:
                    const std::vector<std::queue<T>>* orig_q;
                    std::vector<std::queue<T>> q;
                    int prio;
                public:
                    const_iterator(const std::vector<std::queue<T>>& qq, int pp) : orig_q(&qq), q(qq), prio(pp) {}

                    reference operator*() const { return q[prio].front(); }

                    // Prefix increment
                    const_iterator& operator++() {
                        // move to next element by deleting old element from container
                        q[prio].pop();

                        // if at end of current queue
                        if(q[prio].empty()) {
                            // find next queue with contents
                            while(++prio < q.size())
                                if(!(q[prio].empty()))
                                    break;
                        }
                        return *this; 
                    }
                    // Postfix increment
                    const_iterator operator++(int) { const_iterator tmp = *this; ++(*this); return tmp; }

                    // comparison
                    friend bool operator== (const const_iterator& a, const const_iterator& b) { 
                        return a.orig_q == b.orig_q && a.prio == b.prio;
                    };
                    friend bool operator!= (const const_iterator& a, const const_iterator& b) { return !(a == b); }; 
            };
        private:
            // this schema relies on an implicit conversion from Priority to size_type
            std::vector<std::queue<T>> queues;

            // returns index of highest-priority 
            typename std::vector<std::queue<T>>::size_type getTopQueueIndex() const {
                for(typename std::vector<std::queue<T>>::size_type i = 0; i < queues.size(); i++)
                    if(!queues[i].empty())
                        return i;
                return queues.size();
            }
        public:
            ReadyPriorityQueue(Priority max = MAX_PRIO) : queues(max+1) {}

            bool empty() const {
                return getTopQueueIndex() == queues.size();
            }
            void clear() {
                for(auto& q : queues)
                    while(!q.empty())
                        q.pop();
            }
            typename ReadyPriorityQueue<T>::const_iterator begin() const {
                return ReadyPriorityQueue<T>::const_iterator(queues, getTopQueueIndex());
            }
            typename ReadyPriorityQueue<T>::const_iterator end() const {
                return ReadyPriorityQueue<T>::const_iterator(queues, queues.size());
            }

            Priority getMaxPriority() const {
                return queues.size()-1;
            }

            typename std::queue<T>::size_type size() const {
                typename std::queue<T>::size_type init;
                return std::accumulate(
                    queues.begin(), 
                    queues.end(), 
                    init, 
                    [](typename std::queue<T>::size_type i, std::queue<T> q){ return i + q.size(); });
            }

            void push(T val, Priority p) {
                queues.at(p).push(val);
            }

            const T& front() const {
                return queues.at(getTopQueueIndex()).front();
            }

            void pop() {
                return queues.at(getTopQueueIndex()).pop();
            }
    };

    template<typename T>
    class Timer {
        private:
            T data;
            Step count;
        public:
            Timer(Step cc, T dd) : count(cc), data(dd) {}

            const T& getData() const {
                return data;
            }

            bool step() {
                // assert Timer is not already finished
                assert(count != 0);
                return --count == 0;
            }
    };
}

#endif