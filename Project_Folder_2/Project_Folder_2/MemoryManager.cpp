#include "MemoryManager.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

MemoryManager::MemoryManager(int maxMemory, int memPerProc, int memPerFrame)
    : maxMemory(maxMemory), memPerProc(memPerProc), memPerFrame(memPerFrame) {
    blocks.push_back({ 0, maxMemory, -1 });
}

bool MemoryManager::allocate(int pid) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
        if (it->pid == -1 && it->size() >= memPerProc) {
            int start = it->start;
            int end = start + memPerProc;

            MemoryBlock procBlock = { start, end, pid };

            if (it->size() == memPerProc) {
                *it = procBlock;
            }
            else {
                it->start = end;
                blocks.insert(it, procBlock);
            }
            return true;
        }
    }
    return false;
}

void MemoryManager::deallocate(int pid) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& block : blocks) {
        if (block.pid == pid)
            block.pid = -1;
    }
    mergeFreeBlocks();
}

void MemoryManager::mergeFreeBlocks() {
    for (auto it = blocks.begin(); it != blocks.end() - 1;) {
        if (it->pid == -1 && (it + 1)->pid == -1) {
            it->end = (it + 1)->end;
            blocks.erase(it + 1);
        }
        else {
            ++it;
        }
    }
}

void MemoryManager::dumpSnapshot(int quantumCycle) {
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream filename;
    filename << "memory_stamp_" << std::setw(2) << std::setfill('0') << quantumCycle << ".txt";
    std::ofstream out(filename.str());

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    out << "Timestamp: (" << std::put_time(&tm, "%m/%d/%Y %I:%M:%S%p") << ")\n";

    int procCount = 0;
    for (auto& b : blocks) if (b.pid != -1) procCount++;
    out << "Number of processes in memory: " << procCount << "\n";

    int frag = 0;
    for (auto& b : blocks)
        if (b.pid == -1 && b.size() < memPerProc)
            frag += b.size();
    out << "Total external fragmentation in KB: " << frag / 1024 << "\n\n";

    out << "----end---- = " << maxMemory << "\n\n";
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
        if (it->pid != -1) {
            out << it->end << "\n";
            out << "P" << it->pid << "\n";
            out << it->start << "\n\n";
        }
    }
    out << "----start---- = 0\n";
}
