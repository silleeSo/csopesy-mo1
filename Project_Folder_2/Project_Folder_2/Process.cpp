#include <iostream>
#include <thread>
#include <chrono>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <string>
#include <random>

#include "Process.h"

using namespace std;

void Process::genRandInst(vector<Process::Instruction>& insList) {
    mt19937 rng(static_cast<unsigned>(time(nullptr)));
    uniform_int_distribution<int> countDist(1, 10);    // number of instructions
    uniform_int_distribution<int> opcodeDist(1, 6);    // opcode 1–5
    uniform_int_distribution<int> valueDist(1, 100);   // values for args
    vector<string> vars = { "x", "y", "z", "a", "b" };

    int instCount = countDist(rng);

    for (int i = 0; i < instCount; i++) {
        uint8_t opcode = opcodeDist(rng);
        Process::Instruction instr;
        instr.opcode = opcode;

        switch (opcode) {
        case 1: { // DECLARE(var, value)
            std::string var = vars[rng() % vars.size()];
            instr.args = { var, to_string(valueDist(rng)) };
            break;
        }
        case 2: // ADD(var1, var2/value, var3/value)
        case 3: { // SUBTRACT
            std::string dest = vars[rng() % vars.size()];
            std::string arg1 = rng() % 2 ? vars[rng() % vars.size()] : to_string(valueDist(rng));
            std::string arg2 = rng() % 2 ? vars[rng() % vars.size()] : to_string(valueDist(rng));
            instr.args = { dest, arg1, arg2 };
            break;
        }
        case 4: { // PRINT
            instr.args.push_back("Hello world from processName");
            break;
        }
        case 5: { // SLEEP
            instr.args.push_back(to_string(rng() % 5 + 1)); // 1–5 ticks
            break;
        }
        }

        insList.push_back(instr);
    }

}


Process::Process(size_t pid, string name)
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
        cout << "declared" << endl;
    }

    else if (ins.opcode == 2 && ins.args.size() == 3) { // ADD(var1, var2/value, var3/value)
        const string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) + b);
        cout << "added" << endl;
    }

    else if (ins.opcode == 3 && ins.args.size() == 3) { // SUBTRACT(var1, var2/value, var3/value)
        const string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) - b);
        cout << "sub" << endl;
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
        cout << "sleep" << endl;
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