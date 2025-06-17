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

Process::Process(int pid, string name)
    : pid_(pid), name_(move(name)), finished(false) {
    genRandInst();
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

    else if (ins.opcode == 6 && ins.args.size() == 1) { // FOR(repeats)
        uint16_t repeatCount = getValue(ins.args[0]);

        if (loopStack.size() >= 3) {
            cout << "[Error] Maximum FOR nesting exceeded." << endl;
            return;
        }

        // Save loop state: current instruction index AFTER FOR
        LoopState loop = { insCount_, repeatCount };
        loopStack.push_back(loop);
    }
    else if (ins.opcode == 7) { // END
        if (!loopStack.empty()) {
            auto& loop = loopStack.back();
            loop.repeats--;

            if (loop.repeats > 0) {
                // Jump back to FOR position
                insCount_ = loop.startIns;
            }
            else {
                // Finished loop
                loopStack.pop_back();
            }
        }
        else {
            cout << "[Error] END without matching FOR!" << endl;
        }
    }


}

void Process::genRandInst() {
    insList.clear();
    // Hardcoded values — later we can load these from config.txt
    int min_ins = 1000;
    int max_ins = 2000;

    int totalInstructions = rand() % (max_ins - min_ins + 1) + min_ins;
    vector<string> varPool = { "x", "y", "z", "a", "b", "c" };

    int currentDepth = 0;

    for (int i = 0; i < totalInstructions; ++i) {
        int opcode = rand() % 6 + 1;  // Now allow opcode 1-6
        Instruction ins;
        ins.opcode = opcode;

        if (currentDepth >= 3) {  // Prevent nesting deeper than 3
            if (opcode == 6) opcode = rand() % 5 + 1;
        }

        switch (opcode) {
        case 1: { // DECLARE
            ins.args.push_back(varPool[rand() % varPool.size()]);
            ins.args.push_back(to_string(rand() % 1000));
            break;
        }
        case 2: { // ADD
            ins.args.push_back(varPool[rand() % varPool.size()]);
            ins.args.push_back(varPool[rand() % varPool.size()]);
            ins.args.push_back(to_string(rand() % 100));
            break;
        }
        case 3: { // SUBTRACT
            ins.args.push_back(varPool[rand() % varPool.size()]);
            ins.args.push_back(varPool[rand() % varPool.size()]);
            ins.args.push_back(to_string(rand() % 100));
            break;
        }
        case 4: { // PRINT
            if (rand() % 2) {
                ins.args.push_back("Value: ");
                ins.args.push_back(varPool[rand() % varPool.size()]);
            }
            break;
        }
        case 5: { // SLEEP
            ins.args.push_back(to_string(rand() % 10));
            break;
        }
        case 6: { // FOR
            ins.args.push_back(to_string((rand() % 5) + 1));  // Repeat 1–5 times
            currentDepth++;
            break;
        }
        }

        insList.push_back(ins);

        // Occasionally insert an END after FOR blocks
        if (opcode == 6) {
            // Insert random number of instructions inside FOR loop (simulate subblock)
            int innerBlockSize = rand() % 5 + 1;
            for (int j = 0; j < innerBlockSize; j++) {
                Instruction innerIns;
                int innerOp = rand() % 5 + 1;
                innerIns.opcode = innerOp;

                switch (innerOp) {
                case 1:
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    innerIns.args.push_back(to_string(rand() % 1000));
                    break;
                case 2:
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    innerIns.args.push_back(to_string(rand() % 100));
                    break;
                case 3:
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    innerIns.args.push_back(to_string(rand() % 100));
                    break;
                case 4:
                    innerIns.args.push_back("Inside Loop: ");
                    innerIns.args.push_back(varPool[rand() % varPool.size()]);
                    break;
                case 5:
                    innerIns.args.push_back(to_string(rand() % 10));
                    break;
                }
                insList.push_back(innerIns);
            }

            // Close FOR block with END
            Instruction endIns;
            endIns.opcode = 7;  // END
            insList.push_back(endIns);
            currentDepth--;
        }
    }

    this->insList = insList;
}

bool Process::runOneInstruction() {
    if (insCount_ >= insList.size()) {
        finished = true;
        return false;
    }
    execute(insList[insCount_]);

    // Advance instruction pointer normally, except for END handling
    if (insList[insCount_].opcode != 7 || loopStack.empty() || loopStack.back().repeats == 0)
        insCount_++;

    return true;
}