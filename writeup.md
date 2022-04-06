# Individual Writeup

## Summary

## Analysis
In its current form, the simuation greatly favors strategies which swap out processes immediately upon the completion of a CPU burst, but it discourages swapping out processes prematurely and invoking a relatively costly context swap. As such, the most efficient algorithm is a round robin style system with an effectively infinite time quantum. However, since this is a virtual system with no cost for additional CPUs, the true best algorithm would be a FCFS system with the number of CPUs close to or exceeding the number of processes.

## Development Approach
There's a lot of potential depth to this assignment. On the way to implement one features I always thought of 3 more I wanted to add, which means I never got to add half the stuff I wanted to.

My coding style was inspired by (although likely not enirely in line with) the [ISO C++ Style Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). I aimed to make classes and types more frequently than I normally would. I made liberal use of modern C++ syntax such as range-based for-loops. I used only STL types to avoid the perils of manual memory management. The Simulation system was developed first with a focus on the Round Robin system. The stat collection and export systems were built on top of the simulation. The FCFS system was added last, and I think the process did credit to the robust systems I had built. While adding FCFS took some tweaking to the CPU class, nothing broke downstream in the PCB managmeent, data collection, or data export systems.


## System Analysis
I believe the fundamental simulation and data collection methodology is very sound. The state of Processes and Processors are carefully managed and documented at every system cycle. The flaws of the system really lie in the simplistic program inputs and scheduling algorirthms. With a more representative dataset and more robust scheduling system, the simulation could yield more nuanced results. To this end, the ReadyPriorityQueue class could easily be treated as a base class to derive different scheduling behaviors. Unfortunately, any more complicated approach to CPU swapping would likely require an overhual of the control system.