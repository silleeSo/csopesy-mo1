// Scheduler.h
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <algorithm> // For std::remove_if

#include "Core.h"
#include "Process.h"
#include "ThreadedQueue.h" // For TSQueue
#include "GlobalState.h" // For globalCpuTicks

class Scheduler {
public:
    Scheduler(int num_cpu, const std::string& scheduler_type, uint64_t quantum_cycles,
        uint64_t batch_process_freq, uint64_t min_ins, uint64_t max_ins,
        uint64_t delay_per_exec);
    ~Scheduler();

    void start(); // Start the main scheduler loop
    void stop();  // Stop the main scheduler loop and wait for processes to finish

    void submit(std::shared_ptr<Process> p); // Submit a new process to the ready queue

    void notifyProcessFinished(); // Called by Core when a process finishes
    void requeueProcess(std::shared_ptr<Process> p); // Called by Core when quantum expires or process sleeps

    void startProcessGeneration(); // Begin automatic process generation
    void stopProcessGeneration();  // Stop automatic process generation

    void waitUntilAllDone(); // Blocks until all active processes are finished

    int getNextProcessId(); // Generates a unique PID

    // Getters for Console to display status
    std::vector<std::shared_ptr<Process>> getRunningProcesses() const;
    std::vector<std::shared_ptr<Process>> getFinishedProcesses() const;
    double getCpuUtilization() const;
    int getCoresUsed() const;
    int getCoresAvailable() const;

    // Called by Core to update its utilization metric
    void updateCoreUtilization(int coreId, uint64_t ticksUsed);

private:
    void schedulerLoop(); // Main scheduling thread logic
    void processGeneratorLoop(); // Thread for generating new processes

    int numCpus_;
    std::string schedulerType_;
    uint64_t quantumCycles_;
    uint64_t batchProcessFreq_;
    uint64_t minInstructions_;
    uint64_t maxInstructions_;
    uint64_t delayPerExec_;

    std::vector<std::unique_ptr<Core>> cores_; // CPU cores

    TSQueue<std::shared_ptr<Process>> readyQueue_; // Processes ready to run

    // Vectors for tracking process states, protected by mutexes for concurrent access
    mutable std::mutex runningProcessesMutex_; // Mutable to allow const methods to lock
    std::vector<std::shared_ptr<Process>> runningProcesses_; // Processes currently assigned to cores

    mutable std::mutex finishedProcessesMutex_;
    std::vector<std::shared_ptr<Process>> finishedProcesses_; // Completed processes

    mutable std::mutex sleepingProcessesMutex_;
    std::vector<std::shared_ptr<Process>> sleepingProcesses_; // Processes currently sleeping

    std::thread schedulerThread_;
    std::atomic<bool> running_; // Flag to control schedulerLoop

    std::thread processGenThread_;
    std::atomic<bool> processGenEnabled_; // Flag to control processGeneratorLoop
    std::atomic<uint64_t> lastProcessGenTick_; // Track last generation time

    std::atomic<int> nextPid_; // For generating unique Process IDs
    std::atomic<int> activeProcessesCount_; // Count of processes that are running or in queue/sleeping

    // For CPU Utilization calculation
    std::vector<std::atomic<uint64_t>> coreTicksUsed_; // Tracks ticks each core has been busy
    std::atomic<uint64_t> totalSystemTicks_; // Total ticks the system has been running since scheduler start
    std::atomic<uint64_t> schedulerStartTime_; // Global CPU tick when scheduler started
};