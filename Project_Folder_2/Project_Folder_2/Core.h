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
#include "Process.h";
using namespace std;

class Core {
public:
    Core(int id);

    bool isBusy() const;

    // Called by Scheduler to assign a Process
    bool tryAssign(shared_ptr<Process> p, uint64_t quantum);


    //  Called by Core • executes up to <quantum> instructions
    //  For FCFS  : Core passes UINT64_MAX  (run until done)
    //  For RR    : Core passes time slice (e.g., quantum_cycles)
    //----------------------------------------------------------
    void runQuantum(uint64_t quantum);


private:
    void workerLoop(shared_ptr<Process> p, uint64_t quantum);

    int           id_;
    bool  busy_{ false };
    thread        worker_;
    shared_ptr<Process> runningProcess;

    //FUnction to handle instructions
    void parseInstruction(){
    //TODO: use running process and parse instructions inside!!
    }
};




