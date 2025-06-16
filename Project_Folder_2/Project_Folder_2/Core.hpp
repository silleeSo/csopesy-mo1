#pragma once

#include <memory>
#include <thread>
#include <cstdint>

class Process; // Forward declaration

/*
*   CORE OVERVIEW
    - Tracks whether it's busy or free
    - Can be assigned a process by Scheduler
    - Runs process instructions by calling p->runQuantum(quantum)
    - Works for both RR and FCFS (based on quantum value)
*/

class Core {
public:
    Core(int id);

    // Returns whether the core is currently executing a process
    bool isBusy() const;

    // Called by Scheduler to assign a Process
    bool tryAssign(std::shared_ptr<Process> p, uint64_t quantum);

    // Runs the process for up to <quantum> ticks (FCFS = UINT64_MAX)
    void runQuantum(uint64_t quantum);

private:
    void workerLoop(std::shared_ptr<Process> p, uint64_t quantum);

    // Parses and handles instructions from the running process
    void parseInstruction();

    int id_;
    bool busy_{ false };
    std::thread worker_;
    std::shared_ptr<Process> runningProcess;
};
