#include "Core.h"
#include "Scheduler.h"
#include "GlobalState.h"
#include <iostream>

Core::Core(int id, Scheduler* scheduler, uint64_t delayPerExec)
    : id_(id), busy_(false), scheduler(scheduler), delayPerExec_(delayPerExec) {}

Core::~Core() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool Core::isBusy() const {
    return busy_;
}

bool Core::tryAssign(std::shared_ptr<Process> p, uint64_t quantum) {
    if (busy_) return false;

    if (worker_.joinable()) {
        worker_.join();  // Ensure previous thread is cleanly joined
    }

    runningProcess = p;
    p->setLastCoreId(id_);
    busy_ = true;

    try {
        worker_ = std::thread(&Core::workerLoop, this, p, quantum);
    }
    catch (const std::system_error& e) {
        std::cerr << "[Core-" << id_ << "] Failed to start thread: " << e.what() << std::endl;
        busy_ = false;
        runningProcess = nullptr;
        return false;
    }

    return true;
}

void Core::workerLoop(std::shared_ptr<Process> p, uint64_t quantum) {
    uint64_t executed = 0;
    uint64_t startTicks = globalCpuTicks.load();

    while (!p->isFinished() && executed < quantum) {
        if (p->isSleeping()) {
            if (scheduler) scheduler->requeueProcess(p);
            break;
        }

        bool ran = p->runOneInstruction(id_);
        if (!ran) break;

        executed++;

        if (delayPerExec_ == 0) {
            std::this_thread::yield();
        }
        else {
            uint64_t targetTick = globalCpuTicks.load() + delayPerExec_;
            while (globalCpuTicks.load() < targetTick) {
                std::this_thread::yield();
            }
        }
    }

    if (p->isFinished()) {
        if (scheduler) scheduler->notifyProcessFinished();
    }
    else if (executed >= quantum) {
        if (scheduler) scheduler->requeueProcess(p);
    }

    if (scheduler) {
        uint64_t endTicks = globalCpuTicks.load();
        scheduler->updateCoreUtilization(id_, endTicks - startTicks);
    }

    busy_ = false;
    runningProcess = nullptr;
}
