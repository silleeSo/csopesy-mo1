// Core.cpp
#include "Core.h"
#include "Scheduler.h" // Needed for Scheduler methods being called
#include "GlobalState.h" // Needed for globalCpuTicks

// Removed #include <iostream> to prevent direct console output from Core

// Constructor now accepts delayPerExec
Core::Core(int id, Scheduler* scheduler, uint64_t delayPerExec)
    : id_(id), busy_(false), scheduler(scheduler), delayPerExec_(delayPerExec) {
}

Core::~Core() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool Core::isBusy() const {
    return busy_;
}

bool Core::tryAssign(shared_ptr<Process> p, uint64_t quantum) {
    if (busy_) return false;

    // Ensure any previous worker thread is joined before starting a new one
    if (worker_.joinable()) {
        worker_.join();
    }

    runningProcess = p;
    busy_ = true;
    // Start the worker loop in a new thread
    worker_ = thread(&Core::workerLoop, this, runningProcess, quantum);
    return true;
}

void Core::workerLoop(shared_ptr<Process> p, uint64_t quantum) {
    uint64_t executed = 0;
    uint64_t startTicks = globalCpuTicks.load(); // Record starting ticks for utilization calculation

    if (quantum == UINT64_MAX) { // FCFS Mode
        // cout << "[Core-" << id_ << "] FCFS Mode: Running process " << p->getName() << " to completion." << endl; // Removed for clean console
        while (!p->isFinished()) {
            // Check if process has gone to sleep
            if (p->isSleeping()) {
                // cout << "[Core-" << id_ << "] Process " << p->getName() << " entered SLEEP state. Yielding CPU." << endl; // Removed for clean console
                if (scheduler != nullptr) {
                    scheduler->requeueProcess(p); // Scheduler will move it to sleeping processes
                }
                break; // Core becomes free
            }

            p->runOneInstruction();
            executed++;

            // Apply delay per instruction using busy-waiting on globalCpuTicks
            uint64_t targetTick = globalCpuTicks.load() + delayPerExec_;
            while (globalCpuTicks.load() < targetTick) {
                // Busy-wait or yield thread time
                std::this_thread::yield();
            }
        }
        if (p->isFinished()) {
            // cout << "[Core-" << id_ << "] Process " << p->getName() << " finished under FCFS." << endl; // Removed for clean console
            if (scheduler != nullptr) {
                scheduler->notifyProcessFinished(); // Notify scheduler process is done
            }
        }
    }
    else { // Round Robin Mode
        // cout << "[Core-" << id_ << "] Round Robin Mode: Running process " << p->getName() // Removed for clean console
        //     << " for quantum " << quantum << " instructions." << endl;
        while (executed < quantum && !p->isFinished()) {
            // Check if process has gone to sleep
            if (p->isSleeping()) {
                // cout << "[Core-" << id_ << "] Process " << p->getName() << " entered SLEEP state. Yielding CPU." << endl; // Removed for clean console
                if (scheduler != nullptr) {
                    scheduler->requeueProcess(p); // Scheduler will move it to sleeping processes
                }
                break; // Core becomes free
            }

            p->runOneInstruction();
            executed++;

            // Apply delay per instruction using busy-waiting on globalCpuTicks
            uint64_t targetTick = globalCpuTicks.load() + delayPerExec_;
            while (globalCpuTicks.load() < targetTick) {
                // Busy-wait or yield thread time
                std::this_thread::yield();
            }
        }

        if (p->isFinished()) {
            // cout << "[Core-" << id_ << "] Process " << p->getName() << " finished under Round Robin." << endl; // Removed for clean console
            if (scheduler != nullptr) {
                scheduler->notifyProcessFinished(); // Notify scheduler process is done
            }
        }
        else if (executed >= quantum) { // Quantum expired
            // cout << "[Core-" << id_ << "] Quantum expired for process " << p->getName() // Removed for clean console
            //     << ". Requeuing." << endl;
            if (scheduler != nullptr) {
                scheduler->requeueProcess(p);  // Notify Scheduler that process needs to go back to queue
            }
        }
    }

    // Update core utilization statistics
    if (scheduler != nullptr) {
        uint64_t endTicks = globalCpuTicks.load();
        scheduler->updateCoreUtilization(id_, (endTicks - startTicks));
    }

    busy_ = false; // Core is now free
    runningProcess = nullptr; // Clear the running process pointer
}