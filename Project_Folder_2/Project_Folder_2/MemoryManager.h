#pragma once
#include <vector>
#include <mutex>

// Represents a block in memory
struct MemoryBlock {
    int start;
    int end;
    int pid; // -1 if the block is free

    int size() const { return end - start; }
};

// Thread-safe memory manager
class MemoryManager {
public:
    // Constructor with configuration parameters
    MemoryManager(int maxMemory, int memPerProc, int memPerFrame);

    // Tries to allocate memory for the process. Returns true if successful.
    bool allocate(int pid);

    // Frees memory used by the given process
    void deallocate(int pid);

    // Dumps a snapshot of memory state into memory_stamp_<cycle>.txt
    void dumpSnapshot(int quantumCycle);

private:
    // Merges adjacent free blocks
    void mergeFreeBlocks();

    std::vector<MemoryBlock> blocks;
    std::mutex mtx;

    const int maxMemory;
    const int memPerProc;
    const int memPerFrame;
};
