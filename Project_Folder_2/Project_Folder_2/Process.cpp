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
#include <vector>

#include "Process.h"
#include "GlobalState.h" // Needed for globalCpuTicks

// Static random device and generator for better randomness
static std::random_device rd;
static std::mt19937 gen(rd());

Process::Process(int pid, std::string name)
    : pid_(pid), name_(std::move(name)), finished_(false), isSleeping_(false), sleepTargetTick_(0) {
    // genRandInst() will be called from Scheduler or Console with appropriate min/max
}


void Process::execute(const Instruction& ins) {

    auto getValue = [this](const std::string& token) -> uint16_t {
        if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1)) {
            try {
                return static_cast<uint16_t>(std::stoi(token));
            }
            catch (const std::out_of_range& oor) {
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
        const std::string& var = ins.args[0];
        uint16_t value = ins.args.size() == 2 ? clamp(getValue(ins.args[1])) : 0;
        vars[var] = value;
    }

    else if (ins.opcode == 2 && ins.args.size() == 3) { // ADD(var1, var2/value, var3/value)
        const std::string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) + b);
    }

    else if (ins.opcode == 3 && ins.args.size() == 3) { // SUBTRACT(var1, var2/value, var3/value)
        const std::string& dest = ins.args[0];
        uint16_t a = getValue(ins.args[1]);
        uint16_t b = getValue(ins.args[2]);
        vars[dest] = clamp(static_cast<int64_t>(a) - b);
    }

    else if (ins.opcode == 4) { // PRINT (msg)
        std::string output;
        if (ins.args.empty()) { // Default message
            output = "Hello world from " + name_ + "!";
        }
        else { // Custom message with variable substitution
            for (const std::string& token : ins.args) {
                if (vars.count(token)) {
                    output += std::to_string(vars[token]);
                }
                else {
                    output += token;
                }
            }
        }
        logs_.push_back(output); // Store the log message
        // std::cout << "[Process-" << name_ << "] " << output << std::endl; // For immediate console output, optional (Removed)
    }

    else if (ins.opcode == 5 && ins.args.size() == 1) { // SLEEP(X)
        uint8_t ticks = static_cast<uint8_t>(getValue(ins.args[0]));
        // Set sleep state and target tick
        isSleeping_ = true;
        sleepTargetTick_ = globalCpuTicks.load() + ticks;
        // std::cout << "[Process-" << name_ << "] Entering sleep for " << (int)ticks << " ticks. Target: " << sleepTargetTick_ << std::endl; // Removed for clean console
    }

    else if (ins.opcode == 6 && ins.args.size() == 1) { // FOR(repeats)
        uint16_t repeatCount = getValue(ins.args[0]);

        // The depth check and skipping logic was moved to genRandInst,
        // but this check for loopStack.size() >= 3 *should* ideally not be hit
        // if generation is correct. Still, keep for robustness.
        if (loopStack.size() >= 3) {
            logs_.push_back("[Error] Maximum FOR nesting exceeded during execution. Skipping FOR block.");
            // To properly skip the block, we need to find the matching END
            size_t nestedDepth = 1;
            size_t tempInsCount = insCount_; // Use temp to search
            while (tempInsCount + 1 < insList.size()) {
                tempInsCount++;
                if (insList[tempInsCount].opcode == 6) { // Nested FOR
                    nestedDepth++;
                }
                else if (insList[tempInsCount].opcode == 7) { // END
                    nestedDepth--;
                    if (nestedDepth == 0) { // Found matching END for the current FOR
                        insCount_ = tempInsCount; // Jump instruction pointer past this END
                        return; // Exit execute; runOneInstruction will increment past this END.
                    }
                }
            }
            // If END not found for some reason (malformed instruction list), proceed to next instruction
            return;
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

    std::uniform_int_distribution<uint64_t> distInstructions(min_ins, max_ins);
    uint64_t totalInstructions = distInstructions(gen);

    std::vector<std::string> varPool = { "x", "y", "z", "a", "b", "c" };
    std::uniform_int_distribution<int> distVar(0, varPool.size() - 1);
    std::uniform_int_distribution<int> distValue(0, 1000); // For DECLARE
    std::uniform_int_distribution<int> distSmallValue(0, 100); // For ADD/SUB
    std::uniform_int_distribution<int> distSleepTicks(1, 10); // For SLEEP, at least 1 tick
    std::uniform_real_distribution<double> distProbability(0.0, 1.0); // For weighted chance in PRINT

    // New weighted distribution for opcodes (reduced FOR/SLEEP frequency)
    std::vector<int> opcode_weights = {
        1, 1, 1, 1, // DECLARE (4 units, 20%)
        2, 2, 2, 2, // ADD (4 units, 20%)
        3, 3, 3, 3, // SUBTRACT (4 units, 20%)
        4, 4, 4, 4, // PRINT (4 units, 20%)
        5,          // SLEEP (1 unit, 5%) - significantly reduced
        6           // FOR (1 unit, 5%) - significantly reduced
    }; // Total units = 20

    std::uniform_int_distribution<size_t> distWeightedOpcode(0, opcode_weights.size() - 1);

    int currentDepth = 0; // Tracks logical FOR nesting depth during generation

    for (uint64_t i = 0; i < totalInstructions; ++i) {
        int opcode;
        // Prioritize non-FOR opcodes if at max depth
        if (currentDepth >= 3) {
            // Generate only non-FOR/non-SLEEP ops to avoid excessive nesting or immediate re-sleep
            std::uniform_int_distribution<int> distNonSpecialOpcode(1, 4); // DECLARE, ADD, SUBTRACT, PRINT
            opcode = distNonSpecialOpcode(gen);
        }
        else {
            // Otherwise, use the weighted distribution
            int opcode_index = distWeightedOpcode(gen);
            opcode = opcode_weights[opcode_index];
        }

        Instruction ins;
        ins.opcode = opcode;

        switch (opcode) {
        case 1: { // DECLARE(var, [value])
            ins.args.push_back(varPool[distVar(gen)]);
            if (distValue(gen) % 2 == 0) { // Randomly add a value or not
                ins.args.push_back(std::to_string(distValue(gen)));
            }
            break;
        }
        case 2: { // ADD(var1, var2/value, var3/value)
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(std::to_string(distSmallValue(gen)));
            break;
        }
        case 3: { // SUBTRACT(var1, var2/value, var3/value)
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(varPool[distVar(gen)]);
            ins.args.push_back(std::to_string(distSmallValue(gen)));
            break;
        }
        case 4: { // PRINT(msg)
            if (distProbability(gen) < 0.5) { // 50% chance for custom message
                ins.args.push_back("Value of ");
                ins.args.push_back(varPool[distVar(gen)]);
                ins.args.push_back(": ");
            }
            break;
        }
        case 5: { // SLEEP(X)
            ins.args.push_back(std::to_string(distSleepTicks(gen)));
            break;
        }
        case 6: { // FOR(repeats)
            std::uniform_int_distribution<int> distRepeats(1, 5);
            ins.args.push_back(std::to_string(distRepeats(gen)));  // Repeat 1–5 times
            currentDepth++;
            break;
        }
        }

        insList.push_back(ins);

        // If a FOR instruction was just added, add some inner instructions and an END
        if (opcode == 6) {
            std::uniform_int_distribution<int> distInnerBlockSize(1, 5); // 1 to 5 instructions inside loop
            int innerBlockSize = distInnerBlockSize(gen);

            for (int j = 0; j < innerBlockSize; j++) {
                Instruction innerIns;
                int innerOp;
                // Use weighted distribution for inner ops too, but strictly prevent FOR inside here
                int innerOp_index = distWeightedOpcode(gen);
                innerOp = opcode_weights[innerOp_index];
                while (innerOp == 6) { // Ensure inner loop does not generate FOR, to control depth
                    innerOp_index = distWeightedOpcode(gen);
                    innerOp = opcode_weights[innerOp_index];
                }

                innerIns.opcode = innerOp;

                switch (innerOp) {
                case 1:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(std::to_string(distValue(gen)));
                    break;
                case 2:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(std::to_string(distSmallValue(gen)));
                    break;
                case 3: // FIX: Corrected this line from previous version
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(std::to_string(distSmallValue(gen)));
                    break;
                case 4:
                    if (distProbability(gen) < 0.5) {
                        innerIns.args.push_back("Inside Loop (");
                        innerIns.args.push_back(name_);
                        innerIns.args.push_back("): ");
                        innerIns.args.push_back(varPool[distVar(gen)]);
                    }
                    break;
                case 5:
                    innerIns.args.push_back(std::to_string(distSleepTicks(gen)));
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
            // std::cout << "[Process-" << name_ << "] Woke up from sleep." << std::endl; // Removed for clean console
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

    // Capture the instruction opcode before execution to decide on insCount_ increment
    uint8_t currentOpcode = insList[insCount_].opcode;
    bool wasSleepingBeforeExecution = isSleeping_; // Capture state before execute()

    execute(insList[insCount_]);

    // Decide how to advance insCount_ based on the executed instruction and process state
    if (currentOpcode == 5) { // If the instruction executed was a SLEEP
        // If it successfully put the process to sleep OR it failed (but still consumed the instruction slot)
        insCount_++; // Always advance past the SLEEP instruction
    }
    else if (currentOpcode == 6) { // If it was a FOR instruction
        // insCount_ is already set by the FOR logic in execute() (to startIns - 1)
        // and will be incremented by runOneInstruction's general increment if it falls through.
        // It's handled by the specific FOR/END logic that manipulates insCount_
        // No general increment here as it's directly manipulated in execute for loop jumps.
    }
    else if (currentOpcode == 7) { // If it was an END instruction
        // insCount_ is handled by the END logic in execute().
        // If loop finished, it advances past END.
        // If loop repeated, it sets to startIns - 1.
        // No general increment here as it's directly manipulated in execute for loop jumps.
    }
    else { // For all other instructions (DECLARE, ADD, SUBTRACT, PRINT)
        insCount_++;
    }


    // Check again if process finished after instruction execution
    if (insCount_ >= insList.size()) {
        finished_ = true;
        return false;
    }

    return true;
}