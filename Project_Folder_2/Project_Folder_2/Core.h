/*
*   CORE OVERVIEW
    - Tracks whether it's busy or free
    - Can be assigned a process by Scheduler
    - Runs process instructions by calling p->runQuantum(quantum)
    - Works for both RR and FCFS (based on quantum value)
*/
#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include "Process.h"
using namespace std;

// Forward declare Scheduler to avoid circular include
class Scheduler;

class Core {
public:
    Core(int id, Scheduler* scheduler);  // inject Scheduler reference
    ~Core();

    bool isBusy() const;

    // Called by Scheduler to assign a Process
    bool tryAssign(shared_ptr<Process> p, uint64_t quantum);

    // Called by Core • executes up to <quantum> instructions
    // For FCFS  : Core passes UINT64_MAX (run until done)
    // For RR    : Core passes time slice (e.g., quantum_cycles)
    // void runQuantum(uint64_t quantum);

private:
    void workerLoop(shared_ptr<Process> p, uint64_t quantum);

    int id_;
    atomic<bool> busy_;
    thread worker_;
    shared_ptr<Process> runningProcess;

    Scheduler* scheduler;  // to notify Scheduler if quantum expires
};
