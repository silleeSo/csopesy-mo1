#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include "Process.h";
#include "Core.h";
using namespace std;

/*
    SCHEDULER OVERVIEW
    - Keeps a pool of Core objects
    - Launches a schedulerLoop() thread that:
    - Watches readyQ_
    - Finds a free core
    - Sends process to core->tryAssign(p, quantum_)
    - quantum_ determines if it's RR or FCFS
*/

template <typename T> class TSQueue;  // your thread-safe queue

class Scheduler {
public:
    Scheduler(int numCores, string algo, uint64_t quantum);
    ~Scheduler();

    void start();  // start scheduler thread
    void stop();   // join scheduler thread

    shared_ptr<Process> createProcess(const string& name);

private:
    void schedulerLoop();  // running in background

    string algorithm;     // "fcfs" or "rr"
    uint64_t quantum;       // quantum per slice
    bool running = false;
    thread schedThread;

    vector<shared_ptr<Core>> cores; //oki but shared pointer (what if core children thread ng scheduler)
    TSQueue<shared_ptr<Process>>* readyQ;   // inject or own queue
};
