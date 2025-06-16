#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

/*
* PROCESS CLASS OVERVIEW
*
* - Should have basic process info (for process-smi, screen -ls)
* - Holds list of instructions and runtime state
* - Pure data structure — execution logic handled externally (e.g. by Core)
*/

class Process {
public:
    struct Instruction {
        uint8_t opcode;                // Numeric command ID
        std::vector<std::string> args; // Raw arguments
    };

    // Constructor
    Process(int pid, std::string name);

    // Basic info access
    int pid() const;
    const std::string& name() const;
    bool finished() const;

    // Status summary for display
    std::string smi() const;

    // Instruction access (for Core)
    const std::vector<Instruction>& getInstructions() const;
    std::vector<Instruction>& getMutableInstructions();

    // Program counter access
    size_t getPC() const;
    void advancePC();
    void setPC(size_t newPC);

    // Variable map access
    std::unordered_map<std::string, uint16_t>& getVars();

    // Loop stack access
    std::vector<size_t>& getLoopStack();

    // Finish flag control
    void markFinished();

private:
    int pid_;
    std::string name_;
    bool finished_ = false;

    std::vector<Instruction> code_;               // Instruction list
    size_t pc_ = 0;                               // Program counter
    std::unordered_map<std::string, uint16_t> vars_;   // Variable memory
    std::vector<size_t> loopStack_;              // FOR loop stack
};
