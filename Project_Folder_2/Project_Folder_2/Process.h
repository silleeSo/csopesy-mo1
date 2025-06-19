// Process.h
#pragma once
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <chrono> // For current timestamp in smi()
#include <ctime> // For current timestamp in smi()
#include <iomanip> // For formatting timestamp in smi()

// Removed using namespace std; to be explicit
/*
* PROCESS CLASS OVERVIEW
*
    - should have basic process info (stuff that will appear on process-smi, screen -ls)
    - should have list of instruction
    - process purel data structure, core running functions

*/

class Process {
public:
    struct Instruction {
        uint8_t opcode = 0;          // numeric command ID
        std::vector<std::string> args;       // raw arguments
    };

    struct LoopState {
        size_t startIns;   // Index of FOR instruction in insList
        uint16_t repeats;
    };

    //  Constructor / basic getters

    Process(int pid, std::string name);

    int getPid() const {
        return pid_;
    }
    const std::string& getName() const {
        return name_;
    }
    bool isFinished() const {
        return finished_;
    }
    bool isSleeping() const {
        return isSleeping_;
    }
    uint64_t getSleepTargetTick() const {
        return sleepTargetTick_;
    }
    size_t getCurrentInstructionIndex() const {
        return insCount_;
    }
    size_t getTotalInstructions() const {
        return insList.size();
    }
    const std::vector<std::string>& getLogs() const {
        return logs_;
    }
    const std::unordered_map<std::string, uint16_t>& getVariables() const {
        return vars;
    }


    //  process‑smi helper → returns one‑liner status for Screen
    std::string smi() const {
        std::stringstream ss;
        ss << "Process name: " << name_ << "\n";
        ss << "ID: " << pid_ << "\n";

        // Logs
        ss << "Logs:\n";
        if (logs_.empty()) {
            ss << "  (No logs yet)\n";
        }
        else {
            // Get current time for log entry (example, actual log entries should have their own timestamps)
            time_t now = time(nullptr);
            tm localtm{};
#ifdef _WIN32
            localtime_s(&localtm, &now);
#else
            localtime_r(&now, &localtm);
#endif
            char buf[64];
            strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &localtm);

            for (const auto& logEntry : logs_) {
                ss << "  " << buf << " Core: \"" << logEntry << "\"\n";
            }
        }

        // Status and progress
        if (isFinished()) {
            ss << "Finished!\n";
        }
        else if (isSleeping()) {
            ss << "Status: Sleeping (Until tick: " << sleepTargetTick_ << ")\n";
        }
        else {
            ss << "Status: Running\n"; // Added explicit running status
        }


        ss << "Current instruction line: " << getCurrentInstructionIndex() << "\n";
        ss << "Lines of code: " << getTotalInstructions() << "\n";

        // Display variables (optional, but good for debugging/completeness)
        ss << "Variables:\n";
        if (vars.empty()) {
            ss << "  (No variables declared)\n";
        }
        else {
            for (const auto& pair : vars) {
                ss << "  " << pair.first << " = " << pair.second << "\n";
            }
        }
        return ss.str();
    }

    void execute(const Instruction& ins);
    void genRandInst(uint64_t min_ins, uint64_t max_ins); // Now accepts min/max instructions
    bool runOneInstruction();
    void setIsSleeping(bool val, uint64_t targetTick = 0) {
        isSleeping_ = val;
        sleepTargetTick_ = targetTick;
    }


private:

    //  DATA (must survive pre‑emption)
    int pid_;
    std::string name_;
    bool finished_; // Renamed from 'finished' for clarity with other bools
    bool isSleeping_ = false; // New: indicates if the process is in a SLEEP state
    uint64_t sleepTargetTick_ = 0; // New: The globalCpuTick value when sleep should end

    std::vector<Instruction> insList;         // program
    size_t insCount_ = 0;       // program counter
    std::unordered_map<std::string, uint16_t> vars;         // DECLARE, ADD, SUB
    std::vector<LoopState> loopStack; // loop counters are stored here
    std::vector<std::string> logs_; // New: Stores output from PRINT instructions for screen -s
};