#include "Scheduler.h"
#include "Core.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <iostream>

static std::random_device scheduler_rd;
static std::mt19937 scheduler_gen(scheduler_rd());

Scheduler::Scheduler(int num_cpu, const std::string& scheduler_type, uint64_t quantum_cycles,
    uint64_t batch_process_freq, uint64_t min_ins, uint64_t max_ins, uint64_t delay_per_exec,
    MemoryManager& memoryManager)
    : numCpus_(num_cpu), schedulerType_(scheduler_type), quantumCycles_(quantum_cycles),
    batchProcessFreq_(batch_process_freq), minInstructions_(min_ins), maxInstructions_(max_ins),
    delayPerExec_(delay_per_exec), running_(false), processGenEnabled_(false),
    lastProcessGenTick_(0), nextPid_(1), activeProcessesCount_(0),
    schedulerStartTime_(0), memoryManager_(memoryManager), lastQuantumSnapshot_(0), quantumIndex_(0) {

    cores_.reserve(numCpus_);
    for (int i = 0; i < numCpus_; ++i) {
        cores_.emplace_back(std::make_unique<Core>(i, this, delayPerExec_));
        coreTicksUsed_.emplace_back(std::make_unique<std::atomic<uint64_t>>(0));
    }
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (!running_.load()) {
        running_ = true;
        schedulerStartTime_ = globalCpuTicks.load();
        schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
    }
}

void Scheduler::stop() {
    // Signal all cores to stop their work immediately.
    for (const auto& core : cores_) {
        if (core) {
            core->stop();
        }
    }

    // Stop the scheduler and process generator loops
    running_ = false;
    processGenEnabled_ = false;

    // Join the scheduler's own threads
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    if (processGenThread_.joinable()) {
        processGenThread_.join();
    }
}

void Scheduler::submit(std::shared_ptr<Process> p) {
    bool success = memoryManager_.allocate(p->getPid());
    if (success) {
        readyQueue_.push(p);
        activeProcessesCount_++;
    }
    else {
        memoryPendingQueue.push(p);
        static int warnCount = 0;
        if (warnCount < 10) {
            // std::cout << "[MemoryManager] Process " << p->getName() << " deferred due to insufficient memory.\n";
            warnCount++;
        }
    }
}


void Scheduler::notifyProcessFinished() {
}

void Scheduler::requeueProcess(std::shared_ptr<Process> p) {
    if (p->isSleeping()) {
        std::lock_guard<std::mutex> lock(sleepingProcessesMutex_);
        sleepingProcesses_.push_back(p);
    }
    else {
        readyQueue_.push(p);
    }
}

void Scheduler::startProcessGeneration() {
    if (!processGenEnabled_.load()) {
        processGenEnabled_ = true;
        lastProcessGenTick_ = globalCpuTicks.load();
        processGenThread_ = std::thread(&Scheduler::processGeneratorLoop, this);
    }
}

void Scheduler::stopProcessGeneration() {
    processGenEnabled_ = false;
    if (processGenThread_.joinable()) processGenThread_.join();
}

void Scheduler::waitUntilAllDone() {
    while (activeProcessesCount_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int Scheduler::getNextProcessId() {
    return nextPid_++;
}

std::vector<std::shared_ptr<Process>> Scheduler::getRunningProcesses() const {
    std::vector<std::shared_ptr<Process>> running;
    for (const auto& core : cores_) {
        if (core->isBusy()) {
            auto p = core->getRunningProcess();
            if (p) {
                running.push_back(p);
            }
        }
    }
    return running;
}

std::vector<std::shared_ptr<Process>> Scheduler::getFinishedProcesses() const {
    std::lock_guard<std::mutex> lock(finishedProcessesMutex_);
    return finishedProcesses_;
}

std::vector<std::shared_ptr<Process>> Scheduler::getSleepingProcesses() const {
    std::lock_guard<std::mutex> lock(sleepingProcessesMutex_);
    return sleepingProcesses_;
}

double Scheduler::getCpuUtilization() const {
    if (numCpus_ == 0) return 0.0;

    int busyCores = getCoresUsed();
    double utilization = static_cast<double>(busyCores) / numCpus_ * 100.0;

    return utilization;
}

int Scheduler::getCoresUsed() const {
    return static_cast<int>(std::count_if(cores_.begin(), cores_.end(), [](const auto& core) {
        return core->isBusy();
        }));
}

int Scheduler::getCoresAvailable() const {
    return numCpus_ - getCoresUsed();
}

void Scheduler::updateCoreUtilization(int coreId, uint64_t ticksUsed) {
    if (coreId >= 0 && coreId < numCpus_) {
        coreTicksUsed_[coreId]->fetch_add(ticksUsed);
    }
}

Core* Scheduler::getCore(int index) const {
    if (index >= 0 && index < static_cast<int>(cores_.size())) {
        return cores_[index].get();
    }
    return nullptr;
}

void Scheduler::addFinishedProcess(std::shared_ptr<Process> p) {
    std::lock_guard<std::mutex> lock(finishedProcessesMutex_);
    if (finishedPIDs_.find(p->getPid()) == finishedPIDs_.end()) {
        p->setFinishTime(time(nullptr));
        memoryManager_.deallocate(p->getPid());
        finishedProcesses_.push_back(p);
        finishedPIDs_.insert(p->getPid());
        activeProcessesCount_--;
    }
}



void Scheduler::schedulerLoop() {
    while (running_.load()) {
        {
            std::lock_guard<std::mutex> lock(sleepingProcessesMutex_);
            auto now = globalCpuTicks.load();
            auto it = sleepingProcesses_.begin();
            while (it != sleepingProcesses_.end()) {
                if ((*it)->isSleeping() && now >= (*it)->getSleepTargetTick()) {
                    (*it)->setIsSleeping(false);
                    readyQueue_.push(*it);
                    it = sleepingProcesses_.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        while (!memoryPendingQueue.empty()) {
            std::shared_ptr<Process> p = memoryPendingQueue.front();
            if (memoryManager_.allocate(p->getPid())) {
                readyQueue_.push(p);
                memoryPendingQueue.pop();
            }
            else {
                break;
            }
        }

        for (size_t i = 0; i < cores_.size(); ++i) {
            int index = (nextCoreIndex_ + i) % cores_.size();
            auto& core = cores_[index];

            if (!core->isBusy()) {
                std::shared_ptr<Process> p;
                if (readyQueue_.try_pop(p)) {
                    uint64_t quantum = (schedulerType_ == "rr") ? quantumCycles_ : UINT64_MAX;
                    if (!core->tryAssign(p, quantum)) {
                        std::cout << "[Scheduler] Core-" << index << " failed to assign process " << p->getName() << ". Requeuing.\n";
                        requeueProcess(p);
                    }
                    else {
                        nextCoreIndex_ = (index + 1) % cores_.size();
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(finishedProcessesMutex_);
            for (auto& core : cores_) {
                auto p = core->getRunningProcess();
                if (p && p->isFinished()) {
                    int pid = p->getPid();
                    if (finishedPIDs_.find(pid) == finishedPIDs_.end()) {
                        p->setFinishTime(time(nullptr));
                        memoryManager_.deallocate(pid);
                        finishedProcesses_.push_back(p);
                        finishedPIDs_.insert(pid);
                        activeProcessesCount_--;
                    }
                }
            }
        }

        uint64_t now = globalCpuTicks.load();
        if ((now - lastQuantumSnapshot_) >= quantumCycles_) {
            memoryManager_.dumpSnapshot(quantumIndex_++);
            lastQuantumSnapshot_ = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Scheduler::processGeneratorLoop() {
    while (processGenEnabled_.load()) {
        uint64_t now = globalCpuTicks.load();
        if (now >= lastProcessGenTick_.load() + batchProcessFreq_) {
            int pid = getNextProcessId();
            std::string name = "p" + std::to_string(pid);
            auto proc = std::make_shared<Process>(pid, name);
            proc->genRandInst(minInstructions_, maxInstructions_);
            submit(proc);
            lastProcessGenTick_ = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
