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

    //  Constructor / basic getters
  
    Process(int pid, string name)
        : pid_(pid), name_(move(name)) { /* initialise members if needed */
        //TODO: add a function that randomizes and creates instruction list
    }

    int pid() const { 
        return pid_; 
    }
    const string& name() const { 
        return name_; 
    }
    bool finished() const { 
        return finished; 
    }
    
    //  process‑smi helper → returns one‑liner status for Screen
    string smi() const {
        // TODO: return formatted string with pid, name, pc, etc.
        return "[smi stub]";
    }

private:

    void execute(const Instruction& ins);   // execute ONE instruction
    //uint16_t valueOf(const string& tok);    // map lookup / literal (DO WE NEED THIS)

    //  DATA (must survive pre‑emption)
    int pid_;
    string name_;
    bool finished = false;
    vector<Instruction> code;         // program
    size_t pc_ = 0;       // program counter
    unordered_map<string, uint16_t> vars;         // DECLARE, ADD, SUB
    vector<size_t> loopStack;    // loop counters are stored here

    vector<Instruction> getInstructions() {

    }

};





