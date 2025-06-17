#include <iostream>
#include <thread>
#include <chrono>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <string>

#include "Process.h"

using namespace std;

void genRandInst(vector<Process::Instruction>& insList) {

}


Process::Process(int pid, string name)
    : pid_(pid), name_(move(name)) { /* initialise members if needed */
    //TODO: add a function that randomizes and creates instruction list
    finished = false;
    genRandInst(insList);
}

void Process::execute(const Instruction& ins) {
    
    auto getValue = [this](const string& token) -> uint16_t {
        if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1)) {
            return static_cast<uint16_t>(stoi(token));
        }
        else {
            return vars.count(token) ? vars[token] : 0;
        }
    };

    auto clamp = [](int64_t val) -> uint16_t {
        if (val < 0) return 0;
        if (val > UINT16_MAX) return UINT16_MAX;
        return static_cast<uint16_t>(val);
    };

    if (ins.opcode == 1 && ins.args.size() >= 1) { // DECLARE(var, [value])
        const string& var = ins.args[0];
        uint16_t value = ins.args.size() == 2 ? clamp(getValue(ins.args[1])) : 0;
        vars[var] = value;
    }

    else if (ins.opcode == 2 && ins.args.size() == 3) { // ADD(var1, var2/value, var3/value)
        const string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) + b);
    }

    else if (ins.opcode == 3 && ins.args.size() == 3) { // SUBTRACT(var1, var2/value, var3/value)
        const string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) - b);
    }

    else if (ins.opcode == 4) { // PRINT (msg)
        if (!ins.args.empty()) {
            string output;

            for (const string& token : ins.args) {
                if (vars.count(token)) {
                    output += to_string(vars[token]);
                }
                else {
                    output += token;
                }
            }

            cout << "[Screen-" << name_ << "] " << output << endl;
        }
        else {
            cout << "Hello world from " << name_ << "!" << endl;
        }
    }

    else if (ins.opcode == 5 && ins.args.size() == 1) { // SLEEP(X)
        uint8_t ticks = static_cast<uint8_t>(getValue(ins.args[0]));
        this_thread::sleep_for(chrono::milliseconds(10 * ticks)); // Simulate CPU tick sleep
    }

    /*else if (ins.opcode == 6 && ins.args.size() >= 2) { // FOR (instruction_set_id, repeats)
        const string& loopId = ins.args[0]; // ID or reference to an instruction set
        uint16_t repeatCount = getValue(ins.args[1]);

        if (loopStack.size() >= 3) return; // Support max 3-level nesting
        loopStack.push_back(repeatCount);

        // You need some way to reference instruction sets or subblocks. Example stub:
        vector<Instruction> subInstructions = getLoopBlock(loopId);

        for (int i = 0; i < repeatCount; ++i) {
            for (const Instruction& subIns : subInstructions) {
                execute(subIns); // recursive
            }
        }

        loopStack.pop_back();
    }*/
}