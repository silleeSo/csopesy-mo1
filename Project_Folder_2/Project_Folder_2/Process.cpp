// Process.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <string>
#include <random> // For std::mt19937 and std::uniform_int_distribution

#include "Process.h"
#include "GlobalState.h" // Needed for globalCpuTicks

using namespace std;

// Static random device and generator for better randomness
static random_device rd;
static mt19937 gen(rd());

Process::Process(int pid, string name)
    : pid_(pid), name_(move(name)), finished_(false), isSleeping_(false), sleepTargetTick_(0) {
    // genRandInst() will be called from Scheduler or Console with appropriate min/max
}


void Process::execute(const Instruction& ins) {

    auto getValue = [this](const string& token) -> uint16_t {
        if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1)) {
            try {
                return static_cast<uint16_t>(stoi(token));
            }
            catch (const out_of_range& oor) {
                // Handle overflow/underflow for literals if needed,
                // but clamp will handle it later for variable assignment.
                return 0;
            }
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
        string output;
        if (ins.args.empty()) { // Default message
            output = "Hello world from " + name_ + "!";
        }
        else { // Custom message with variable substitution
            for (const string& token : ins.args) {
                if (vars.count(token)) {
                    output += to_string(vars[token]);
                }
                else {
                    output += token;
                }
            }
        }
        logs_.push_back(output); // Store the log message
        // cout << "[Process-" << name_ << "] " << output << endl; // For immediate console output, optional
    }

    else if (ins.opcode == 5 && ins.args.size() == 1) { // SLEEP(X)
        uint8_t ticks = static_cast<uint8_t>(getValue(ins.args[0]));
        // Set sleep state and target tick
        isSleeping_ = true;
        sleepTargetTick_ = globalCpuTicks.load() + ticks;
        cout << "[Process-" << name_ << "] Entering sleep for " << (int)ticks << " ticks. Target: " << sleepTargetTick_ << endl;
    }

    else if (ins.opcode == 6 && ins.args.size() == 1) { // FOR(repeats)
        uint16_t repeatCount = getValue(ins.args[0]);

        if (loopStack.size() >= 3) {
            logs_.push_back("[Error] Maximum FOR nesting exceeded. Skipping FOR block.");
            // To properly skip the block, we need to find the matching END
            size_t nestedDepth = 1;
            size_t originalInsCount = insCount_;
            while (originalInsCount + 1 < insList.size()) {
                originalInsCount++;
                if (insList[originalInsCount].opcode == 6) { // Nested FOR
                    nestedDepth++;
                }
                else if (insList[originalInsCount].opcode == 7) { // END
                    nestedDepth--;
                    if (nestedDepth == 0) { // Found matching END for the current FOR
                        insCount_ = originalInsCount; // Jump instruction pointer past this END
                        break;
                    }
                }
            }
            return; // Exit execute, runOneInstruction will increment past this FOR
        }

        // Save loop state: current instruction index AFTER FOR (so insCount_++ will point to first instruction inside loop)
        LoopState loop = { insCount_ + 1, repeatCount };
        loopStack.push_back(loop);
    }
    else if (ins.opcode == 7) { // END
        if (!loopStack.empty()) {
            LoopState& currentLoop = loopStack.back(); // Use reference to modify
            currentLoop.repeats--;

            if (currentLoop.repeats > 0) {
                // Jump back to FOR position (which is stored as the instruction *after* the FOR)
                insCount_ = currentLoop.startIns - 1; // -1 because runOneInstruction will increment it
            }
            else {
                // Finished loop, pop from stack
                loopStack.pop_back();
            }
        }
        else {
            logs_.push_back("[Error] END without matching FOR!");
        }
    }
}

// Updated to accept min_ins and max_ins from config
void Process::genRandInst(uint64_t min_ins, uint64_t max_ins) {
    insList.clear();
    logs_.clear(); // Clear logs for new process generation
    vars.clear(); // Clear variables
    loopStack.clear(); // Clear loop stack
    insCount_ = 0; // Reset program counter

    uniform_int_distribution<uint64_t> distInstructions(min_ins, max_ins);
    int totalInstructions = distInstructions(gen); // Use the provided range

    vector<string> varPool = { "x", "y", "z", "a", "b", "c" };
    uniform_int_distribution<int> distOpcode(1, 6); // Opcodes 1-6
    uniform_int_distribution<int> distVar(0, varPool.size() - 1);
    uniform_int_distribution<int> distValue(0, 1000); // For DECLARE
    uniform_int_distribution<int> distSmallValue(0, 100); // For ADD/SUB
    uniform_int_distribution<int> distSleepTicks(1, 10); // For SLEEP, at least 1 tick

    int currentDepth = 0;

    for (int i = 0; i < totalInstructions; ++i) {
        int opcode = distOpcode(gen);
        Instruction ins;
        ins.opcode = opcode;

        if (currentDepth >= 3 && opcode == 6) { // Prevent nesting deeper than 3, change FOR to something else
            opcode = distOpcode(gen); // Re-roll opcode
            while (opcode == 6) opcode = distOpcode(gen); // Ensure it's not FOR
            ins.opcode = opcode;
        }

        switch (opcode) {
        case 1: { // DECLARE(var, [value])
            ins.args.push_back(varPool[distVar(gen)]);
            if (distValue(gen) % 2 == 0) { // Randomly add a value or not
                ins.args.push_back(to_string(distValue(gen)));
            }
            break;
        }
        case 2: { // ADD(var1, var2/value, var3/value)
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(to_string(distSmallValue(gen)));
            break;
        }
        case 3: { // SUBTRACT(var1, var2/value, var3/value)
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(to_string(distSmallValue(gen)));
            break;
        }
        case 4: { // PRINT(msg)
            // Randomly choose between default message or custom with variable
            if (distOpcode(gen) % 2 == 0) { // 50% chance for custom message
                ins.args.push_back("Value of ");
                ins.args.push_back(varPool[distVar(gen)]);
                ins.args.push_back(": ");
            }
            // If args are empty, execute will use default "Hello world from <process_name>"
            break;
        }
        case 5: { // SLEEP(X)
            ins.args.push_back(to_string(distSleepTicks(gen)));
            break;
        }
        case 6: { // FOR(repeats)
            uniform_int_distribution<int> distRepeats(1, 5);
            ins.args.push_back(to_string(distRepeats(gen)));  // Repeat 1–5 times
            currentDepth++;
            break;
        }
        }

        insList.push_back(ins);

        // If a FOR instruction was just added, add some inner instructions and an END
        if (opcode == 6) {
            uniform_int_distribution<int> distInnerBlockSize(1, 5); // 1 to 5 instructions inside loop
            int innerBlockSize = distInnerBlockSize(gen);

            for (int j = 0; j < innerBlockSize; j++) {
                Instruction innerIns;
                int innerOp = distOpcode(gen);
                while (innerOp == 6) innerOp = distOpcode(gen); // Prevent nested FORs that aren't closed

                innerIns.opcode = innerOp;

                switch (innerOp) {
                case 1:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(to_string(distValue(gen)));
                    break;
                case 2:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(to_string(distSmallValue(gen)));
                    break;
                case 3:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(to_string(distSmallValue(gen)));
                    break;
                case 4:
                    if (distOpcode(gen) % 2 == 0) {
                        innerIns.args.push_back("Inside Loop (");
                        innerIns.args.push_back(name_);
                        innerIns.args.push_back("): ");
                        innerIns.args.push_back(varPool[distVar(gen)]);
                    }
                    break;
                case 5:
                    innerIns.args.push_back(to_string(distSleepTicks(gen)));
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
}


bool Process::runOneInstruction() {
    if (finished_) {
        return false;
    }

    if (isSleeping_) {
        // If process is sleeping and target tick has passed, wake it up
        if (globalCpuTicks.load() >= sleepTargetTick_) {
            isSleeping_ = false;
            sleepTargetTick_ = 0;
            cout << "[Process-" << name_ << "] Woke up from sleep." << endl;
        }
        else {
            // Still sleeping, do not execute instruction
            return false; // Indicate that no instruction was executed
        }
    }

    if (insCount_ >= insList.size()) {
        finished_ = true;
        return false;
    }

    execute(insList[insCount_]);

    // Only advance instruction pointer if not currently sleeping or if END instruction completes a loop iteration
    // The FOR/END logic in execute() can modify insCount_ directly for jumping.
    // If it's a regular instruction or an END that caused a jump, increment for the *next* instruction.
    // If it's an END that finished a loop, the insCount_ is already set to the instruction after the END.
    if (!isSleeping_ && (insList[insCount_].opcode != 7 || loopStack.empty() || loopStack.back().repeats == 0)) {
        insCount_++;
    }

    // Check again if process finished after instruction execution
    if (insCount_ >= insList.size()) {
        finished_ = true;
        return false;
    }

    return true;
}