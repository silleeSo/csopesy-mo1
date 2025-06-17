#include "Core.h"
#include "Scheduler.h"

#include <iostream>

Core::Core(int id, Scheduler* scheduler)
    : id_(id), busy_(false), scheduler(scheduler) {
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

    if (worker_.joinable()) {
        worker_.join();
    }

    runningProcess = p;
    busy_ = true;
    worker_ = thread(&Core::workerLoop, this, runningProcess, quantum);
    return true;
}

void Core::workerLoop(shared_ptr<Process> p, uint64_t quantum) {
    uint64_t executed = 0;

    if (quantum == UINT64_MAX) {
        cout << "[Core-" << id_ << "] FCFS Mode: Running process " << p->getName() << " to completion." << endl;
        while (!p->isFinished()) {
            p->runOneInstruction();
            executed++;
            cout << "[Core-" << id_ << "] Executed instruction " << executed << endl;
        }
        cout << "[Core-" << id_ << "] Process " << p->getName() << " finished under FCFS." << endl;
    }
    else {
        cout << "[Core-" << id_ << "] Round Robin Mode: Running process " << p->getName()
            << " for quantum " << quantum << " instructions." << endl;
        while (executed < quantum) {
            if (!p->runOneInstruction()) break;
            executed++;
            cout << "[Core-" << id_ << "] Executed instruction " << executed << endl;
        }

        if (scheduler != nullptr) {
            scheduler->notifyProcessFinished();
        }
        else {
            cout << "[Core-" << id_ << "] Quantum expired for process " << p->getName()
                << ". Returning to scheduler." << endl;
            if (scheduler != nullptr) {
                scheduler->requeueProcess(p);  // notify Scheduler that process needs to go back to queue
            }
        }
    }

    busy_ = false;
}
