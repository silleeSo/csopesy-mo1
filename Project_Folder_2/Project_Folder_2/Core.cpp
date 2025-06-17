#include "Core.h"
#include <iostream>

Core::Core(int id) : id_(id), busy_(false) {}

bool Core::isBusy() const {
    return busy_;
}

bool Core::tryAssign(shared_ptr<Process> p, uint64_t quantum) {
    if (busy_) return false; // cannot assign if already busy
    runningProcess = p;
    runQuantum(quantum);
    return true;
}

void Core::runQuantum(uint64_t quantum) {
    busy_ = true;
    worker_ = thread(&Core::workerLoop, this, runningProcess, quantum);
    worker_.detach();
}

void Core::workerLoop(shared_ptr<Process> p, uint64_t quantum) {
    uint64_t executed = 0;
    while (executed < quantum) {
        if (!p->runOneInstruction()) break;
        executed++;
    }
    if (p->isFinished()) {
        cout << "[Core-" << id_ << "] Process " << p->getName() << " finished." << endl;
    }
    busy_ = false;
}
