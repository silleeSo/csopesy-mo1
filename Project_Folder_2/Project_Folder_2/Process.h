#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
using namespace std;
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
        uint8_t opcode;            // numeric command ID
        vector<string> args;       // raw arguments
    };

    struct LoopState {
        size_t startIns;   // Index of FOR instruction in insList
        uint16_t repeats;
    };

    //  Constructor / basic getters

    Process(int pid, string name);

    int getPid() const {
        return pid_;
    }
    const string& getName() const {
        return name_;
    }
    bool isFinished() const {
        return finished;
    }

    //  process‑smi helper → returns one‑liner status for Screen
    string smi() const {
        // TODO: return formatted string with pid, name, pc, etc.
        return "[smi stub]";
    }

    void execute(const Instruction& ins);
    void genRandInst(vector<Instruction>& insList);
    bool runOneInstruction();

private:

    //void execute(const Instruction& ins);   // execute ONE instruction
    //uint16_t valueOf(const string& tok);    // map lookup / literal (DO WE NEED THIS)

    //  DATA (must survive pre‑emption)
    int pid_;
    string name_;
    bool finished;
    vector<Instruction> insList;         // program
    size_t insCount_ = 0;       // program counter
    unordered_map<string, uint16_t> vars;         // DECLARE, ADD, SUB
    std::vector<LoopState> loopStack; // loop counters are stored here

    
    

    

};





