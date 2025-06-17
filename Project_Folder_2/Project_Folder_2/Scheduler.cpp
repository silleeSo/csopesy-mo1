#include "Scheduler.h"


Scheduler::Scheduler(int numCores, string algo, uint64_t quantum_)
    : algorithm(std::move(algo)), quantum(quantum_), running(false)
{
    for (int i = 0; i < numCores; i++) {
        cores.push_back(make_shared<Core>(i, this));
    }
    readyQ = new TSQueue<shared_ptr<Process>>();
}

Scheduler::~Scheduler() {
    stop();
    if (readyQ) {
        delete readyQ;
    }
}

void Scheduler::start() {
    running = true;
    schedThread = thread(&Scheduler::schedulerLoop, this);
}

void Scheduler::stop() {
    running = false;
    if (schedThread.joinable()) {
        schedThread.join();
    }
}

shared_ptr<Process> Scheduler::createProcess(const string& name) {
    auto p = make_shared<Process>(name);
    readyQ->push(p);  // <-- changed enqueue() to push()
    return p;
}

void Scheduler::schedulerLoop() {
    while (running) {
        // Wait (block) until there is at least one process available
        shared_ptr<Process> nextProc = readyQ->pop();

        bool assigned = false;

        // Keep trying to assign to a free core
        while (!assigned) {
            for (auto& core : cores) {
                if (!core->isBusy()) {
                    uint64_t slice = (algorithm == "rr") ? quantum : UINT64_MAX;
                    assigned = core->tryAssign(nextProc, slice);
                    break; // break the for loop if assigned
                }
            }

            if (!assigned) {
                // No free core found, wait a bit and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void Scheduler::requeueProcess(shared_ptr<Process> p) {
    readyQ->push(p);
}

void Scheduler::submit(shared_ptr<Process> p) {
    readyQ->push(p);
    totalProcesses++;
}

void Scheduler::notifyProcessFinished() {
    finishedProcesses++;
}

void Scheduler::waitUntilAllDone() {
    while (finishedProcesses < totalProcesses) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stop();
}
