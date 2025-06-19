// Scheduler.cpp
#include "Scheduler.h"
#include <algorithm> // For std::remove_if
#include <chrono> // For std::chrono::milliseconds
#include <random> // For std::mt19937 and std::uniform_int_distribution
// Removed #include <iostream> to prevent direct console output from Scheduler

// Static random device and generator for process generation
static std::random_device scheduler_rd;
static std::mt19937 scheduler_gen(scheduler_rd());

Scheduler::Scheduler(int num_cpu, const std::string& scheduler_type, uint64_t quantum_cycles,
    uint64_t batch_process_freq, uint64_t min_ins, uint64_t max_ins,
    uint64_t delay_per_exec)
    : numCpus_(num_cpu),
    schedulerType_(scheduler_type),
    quantumCycles_(quantum_cycles),
    batchProcessFreq_(batch_process_freq),
    minInstructions_(min_ins),
    maxInstructions_(max_ins),
    delayPerExec_(delay_per_exec),
    running_(false),
    processGenEnabled_(false),
    lastProcessGenTick_(0),
    nextPid_(1), // Start PIDs from 1
    activeProcessesCount_(0),
    totalSystemTicks_(0),
    schedulerStartTime_(0)
{
    cores_.reserve(numCpus_);
    // Initialize cores and pass 'this' scheduler instance to them
    for (int i = 0; i < numCpus_; ++i) {
        cores_.push_back(std::make_unique<Core>(i, this, delayPerExec_));
    }

    // New explicit initialization for std::vector<std::atomic<uint64_t>>
    // This avoids any potential implicit copy issues with resize.
    coreTicksUsed_.clear(); // Ensure empty before adding
    for (int i = 0; i < numCpus_; ++i) {
        coreTicksUsed_.push_back(std::make_unique<std::atomic<uint64_t>>(0));
    }

}

Scheduler::~Scheduler() {
    stop(); // Ensure all threads are stopped and joined
}

void Scheduler::start() {
    if (!running_.load()) {
        running_ = true;
        // Start the main scheduler thread
        schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
        schedulerThread_.detach(); // Run independently

        // Record the start time of the scheduler for utilization calculation
        schedulerStartTime_ = globalCpuTicks.load();
    }
}

void Scheduler::stop() {
    if (running_.load()) {
        running_ = false; // Signal scheduler loop to stop
        processGenEnabled_ = false; // Also stop process generation

        // Join the scheduler thread if it's joinable (it should be detached, but good practice)
        if (schedulerThread_.joinable()) {
            // std::cout << "[Scheduler] Joining scheduler thread." << std::endl; // Debugging - REMOVED
            schedulerThread_.join();
        }
        // Join the process generator thread
        if (processGenThread_.joinable()) {
            // std::cout << "[Scheduler] Joining process generator thread." << std::endl; // Debugging - REMOVED
            processGenThread_.join();
        }

        // Wait for all cores to finish any assigned processes (optional, but good for clean shutdown)
        for (const auto& core : cores_) {
            if (core->isBusy()) {
                // In a real system, you might send a signal to cores to stop or interrupt.
                // For this simulation, we just wait for current task to finish.
                // This might block indefinitely if a process loops forever without sleeping/finishing.
                // For now, assuming processes will eventually finish or yield.
                // std::cout << "[Scheduler] Waiting for Core-" << core->id_ << " to finish." << std::endl; // Debugging - REMOVED
            }
        }
    }
}

void Scheduler::submit(std::shared_ptr<Process> p) {
    readyQueue_.push(p);
    activeProcessesCount_++;
    // std::cout << "[Scheduler] Process " << p->getName() << " (PID: " << p->getPid() << ") submitted. Total active: " << activeProcessesCount_.load() << std::endl; // REMOVED for clean console
}

void Scheduler::notifyProcessFinished() {
    // This function is called by the Core *after* it detects a process is finished.
    // The decrement of activeProcessesCount_ should ideally happen here or in a centralized
    // place where processes transition to the finished state.
    // We will adjust the schedulerLoop to manage runningProcesses_ more explicitly and
    // decrement activeProcessesCount_ when moving a process to finishedProcesses_.
    // For now, let's remove the decrement here to avoid double-decrement if the schedulerLoop also does it.
    // std::cout << "[Scheduler] Process finished. Total active: " << activeProcessesCount_.load() << std::endl; // REMOVED for clean console
}

void Scheduler::requeueProcess(std::shared_ptr<Process> p) {
    if (p->isSleeping()) {
        std::unique_lock<std::mutex> lock(sleepingProcessesMutex_);
        sleepingProcesses_.push_back(p);
        // std::cout << "[Scheduler] Process " << p->getName() << " (PID: " << p->getPid() << ") moved to sleeping queue." << std::endl; // REMOVED for clean console
    }
    else {
        readyQueue_.push(p);
        // std::cout << "[Scheduler] Process " << p->getName() << " (PID: " << p->getPid() << ") requeued to ready queue." << std::endl; // REMOVED for clean console
    }
}

void Scheduler::startProcessGeneration() {
    if (!processGenEnabled_.load()) {
        processGenEnabled_ = true;
        lastProcessGenTick_ = globalCpuTicks.load(); // Initialize last generation tick
        processGenThread_ = std::thread(&Scheduler::processGeneratorLoop, this);
        processGenThread_.detach(); // Run independently
        // std::cout << "[Scheduler] Process generation started." << std::endl; // REMOVED for clean console
    }
}

void Scheduler::stopProcessGeneration() {
    if (processGenEnabled_.load()) {
        processGenEnabled_ = false;
        if (processGenThread_.joinable()) {
            processGenThread_.join(); // Ensure the generator thread stops gracefully
        }
        // std::cout << "[Scheduler] Process generation stopped." << std::endl; // REMOVED for clean console
    }
}

void Scheduler::waitUntilAllDone() {
    // std::cout << "[Scheduler] Waiting for all " << activeProcessesCount_.load() << " active processes to finish..." << std::endl; // REMOVED for clean console
    // Wait until all processes (running, ready, sleeping) are completed
    while (activeProcessesCount_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep to prevent busy-waiting
    }
    // std::cout << "[Scheduler] All processes finished." << std::endl; // REMOVED for clean console
}

int Scheduler::getNextProcessId() {
    return nextPid_++; // Atomic increment and return
}

std::vector<std::shared_ptr<Process>> Scheduler::getRunningProcesses() const {
    std::unique_lock<std::mutex> lock(runningProcessesMutex_);
    // Filter out finished or sleeping processes from the internal runningProcesses_ list
    std::vector<std::shared_ptr<Process>> currentRunning;
    for (const auto& p : runningProcesses_) {
        if (!p->isFinished() && !p->isSleeping()) {
            currentRunning.push_back(p);
        }
    }
    return currentRunning;
}

std::vector<std::shared_ptr<Process>> Scheduler::getFinishedProcesses() const {
    std::unique_lock<std::mutex> lock(finishedProcessesMutex_);
    return finishedProcesses_;
}

std::vector<std::shared_ptr<Process>> Scheduler::getSleepingProcesses() const {
    std::unique_lock<std::mutex> lock(sleepingProcessesMutex_);
    return sleepingProcesses_;
}

double Scheduler::getCpuUtilization() const {
    uint64_t currentSystemTicks = globalCpuTicks.load() - schedulerStartTime_.load();
    if (currentSystemTicks == 0 || numCpus_ == 0) return 0.0; // Avoid division by zero

    uint64_t totalCoresBusyTicks = 0;
    for (int i = 0; i < numCpus_; ++i) { // Iterate through cores for their current busy state
        totalCoresBusyTicks += coreTicksUsed_[i]->load(); // Accumulate individual core busy ticks
    }

    // Utilization = (Total busy ticks across all cores) / (Total possible ticks * Number of CPUs)
    // Capped at 100% as sometimes the tick measurement might be slightly off
    return std::min(100.0, (static_cast<double>(totalCoresBusyTicks) / (static_cast<double>(currentSystemTicks) * numCpus_)) * 100.0);
}

int Scheduler::getCoresUsed() const {
    int count = 0;
    for (const auto& core : cores_) {
        if (core->isBusy()) {
            count++;
        }
    }
    return count;
}

int Scheduler::getCoresAvailable() const {
    return numCpus_ - getCoresUsed();
}

void Scheduler::updateCoreUtilization(int coreId, uint64_t ticksUsed) {
    if (coreId >= 0 && coreId < numCpus_) {
        coreTicksUsed_[coreId]->fetch_add(ticksUsed); // Atomically add ticks used by this core
    }
}

void Scheduler::schedulerLoop() {
    // std::cout << "[Scheduler] Main scheduler loop started." << std::endl; // REMOVED for clean console
    while (running_.load()) {
        // 1. Handle sleeping processes: Check if any sleeping processes should wake up
        {
            std::unique_lock<std::mutex> lock(sleepingProcessesMutex_);
            // Use std::remove_if to move woken processes to a temporary list
            std::vector<std::shared_ptr<Process>> wokenProcesses;
            sleepingProcesses_.erase(std::remove_if(sleepingProcesses_.begin(), sleepingProcesses_.end(),
                [&](std::shared_ptr<Process> p) {
                    if (p->isSleeping() && globalCpuTicks.load() >= p->getSleepTargetTick()) {
                        p->setIsSleeping(false); // Wake up the process
                        wokenProcesses.push_back(p); // Add to list to be re-queued
                        return true; // Remove from sleepingProcesses_
                    }
                    return false; // Keep in sleepingProcesses_
                }),
                sleepingProcesses_.end());

            // Re-queue woken processes
            for (const auto& p : wokenProcesses) {
                readyQueue_.push(p);
                // std::cout << "[Scheduler] Process " << p->getName() << " (PID: " << p->getPid() << ") woke up and re-queued." << std::endl; // REMOVED for clean console
            }
        } // sleepingProcessesMutex_ unlocked

        // 2. Assign processes to free cores
        // Iterate through cores and try to assign a process if free
        for (const auto& core : cores_) {
            if (!core->isBusy()) {
                std::shared_ptr<Process> p;
                if (readyQueue_.try_pop(p)) { // Try to get a process from the ready queue
                    // Remove process from `runningProcesses_` if it was there (e.g., pre-empted/slept)
                    // and then add it back as it's now being assigned to a core.
                    {
                        std::unique_lock<std::mutex> lock(runningProcessesMutex_);
                        // Erase-remove idiom for shared_ptr vector
                        runningProcesses_.erase(std::remove(runningProcesses_.begin(), runningProcesses_.end(), p), runningProcesses_.end());
                        runningProcesses_.push_back(p); // Add (or re-add) to running list
                    }

                    uint64_t quantumToUse = (schedulerType_ == "rr") ? quantumCycles_ : UINT64_MAX;
                    core->tryAssign(p, quantumToUse);
                    // std::cout << "[Scheduler] Assigned process " << p->getName() << " (PID: " << p->getPid() << ") to Core-" << core->id_ << std::endl; // REMOVED for clean console
                }
            }
        }

        // 3. Clean up and move finished processes from running list to finished list
        // This is necessary because notifyProcessFinished doesn't directly remove from runningProcesses_.
        {
            std::unique_lock<std::mutex> runningLock(runningProcessesMutex_);
            std::unique_lock<std::mutex> finishedLock(finishedProcessesMutex_); // Acquire lock for finished processes

            runningProcesses_.erase(std::remove_if(runningProcesses_.begin(), runningProcesses_.end(),
                [&](std::shared_ptr<Process> p) {
                    if (p->isFinished()) {
                        finishedProcesses_.push_back(p); // Move to finished
                        activeProcessesCount_--; // Decrement active count as it's truly done
                        // std::cout << "[Scheduler] Process " << p->getName() << " (PID: " << p->getPid() << ") moved to finished. Active count: " << activeProcessesCount_.load() << std::endl; // REMOVED for clean console
                        return true; // Remove from running
                    }
                    return false;
                }),
                runningProcesses_.end());
        } // Locks released

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Prevent busy-waiting in scheduler itself
    }
    // std::cout << "[Scheduler] Main scheduler loop stopped." << std::endl; // REMOVED for clean console
}

void Scheduler::processGeneratorLoop() {
    // std::cout << "[Scheduler] Process generator loop started." << std::endl; // REMOVED for clean console
    while (processGenEnabled_.load()) {
        uint64_t currentTicks = globalCpuTicks.load();
        if (currentTicks >= lastProcessGenTick_.load() + batchProcessFreq_) { // Check against the frequency
            // Generate a new process
            std::string newProcessName = "p" + std::to_string(getNextProcessId());
            auto newProcess = std::make_shared<Process>(nextPid_.load() - 1, newProcessName); // PID is already incremented
            newProcess->genRandInst(minInstructions_, maxInstructions_);
            submit(newProcess);
            lastProcessGenTick_ = currentTicks; // Update last generation time
            // std::cout << "[Scheduler] Generated new process: " << newProcessName << ". Total active: " << activeProcessesCount_.load() << std::endl; // REMOVED for clean console
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Check periodically
    }
    // std::cout << "[Scheduler] Process generator loop stopped." << std::endl; // REMOVED for clean console
}