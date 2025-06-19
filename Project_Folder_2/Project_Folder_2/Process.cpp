// Process.cpp
#include <iostream> // Keep for debug if needed, but remove for final
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
    }

    else if (ins.opcode == 5 && ins.args.size() == 1) { // SLEEP(X)
        uint8_t ticks = static_cast<uint8_t>(getValue(ins.args[0]));
        isSleeping_ = true;
        sleepTargetTick_ = globalCpuTicks.load() + ticks;
    }

    else if (ins.opcode == 6 && ins.args.size() == 1) { // FOR(repeats)
        uint16_t repeatCount = getValue(ins.args[0]);

        // This check is a fallback. Generation should prevent this.
        if (loopStack.size() >= 3) {
            logs_.push_back("[Error] Maximum FOR nesting exceeded during execution. Skipping this FOR instruction.");
            // To prevent immediate re-hit, advance program counter past this FOR.
            // runOneInstruction will handle the increment, so just return here.
            return;
        }

        LoopState loop = { insCount_ + 1, repeatCount }; // Start after this FOR instruction
        loopStack.push_back(loop);
    }
    else if (ins.opcode == 7) { // END
        if (!loopStack.empty()) {
            LoopState& currentLoop = loopStack.back();
            currentLoop.repeats--;

            if (currentLoop.repeats > 0) {
                insCount_ = currentLoop.startIns - 1; // Jump back to before the first instruction of the loop block
            }
            else {
                loopStack.pop_back(); // Loop finished, remove from stack
            }
        }
        else {
            logs_.push_back("[Error] END without matching FOR! This indicates a program generation error.");
            // If END without matching FOR is hit, it implies a corrupted program structure.
            // To prevent infinite loops here, we MUST ensure insCount_ advances.
            // The default increment in runOneInstruction will handle this.
        }
    }
}

// Updated to accept min_ins and max_ins from config
void Process::genRandInst(uint64_t min_ins, uint64_t max_ins) {
    insList.clear();
    logs_.clear(); // Clear logs for new process generation
    vars.clear(); // Clear variables
    loopStack.clear(); // Clear loop stack (important for regeneration)
    insCount_ = 0; // Reset program counter

    std::uniform_int_distribution<uint64_t> distInstructions(min_ins, max_ins);
    uint64_t totalInstructions = distInstructions(gen);

    std::vector<std::string> varPool = { "x", "y", "z", "a", "b", "c" };
    std::uniform_int_distribution<int> distVar(0, varPool.size() - 1);
    std::uniform_int_distribution<int> distValue(0, 1000); // For DECLARE
    std::uniform_int_distribution<int> distSmallValue(0, 100); // For ADD/SUB
    std::uniform_int_distribution<int> distSleepTicks(1, 10); // For SLEEP, at least 1 tick
    std::uniform_real_distribution<double> distProbability(0.0, 1.0); // For weighted chance in PRINT

    // Weighted distribution for general opcodes
    std::vector<int> opcode_pool = { 1, 2, 3, 4, 5 }; // DECLARE, ADD, SUBTRACT, PRINT, SLEEP
    std::uniform_int_distribution<size_t> distGeneralOp(0, opcode_pool.size() - 1);

    int currentDepth = 0; // Tracks logical FOR nesting depth during generation

    // Loop until we have generated enough instructions, considering FOR blocks add multiple instructions
    uint64_t instructionsGenerated = 0;
    while (instructionsGenerated < totalInstructions) {
        int opcode;

        // Decide if we should generate a FOR, ensuring we don't exceed depth
        if (currentDepth < 3 && distProbability(gen) < 0.15) { // 15% chance for a FOR if not at max depth
            opcode = 6; // FOR
        }
        else {
            // Otherwise, pick from general ops (DECLARE, ADD, SUBTRACT, PRINT, SLEEP)
            opcode = opcode_pool[distGeneralOp(gen)];
        }

        Instruction ins;
        ins.opcode = opcode;

        switch (opcode) {
        case 1: { // DECLARE(var, [value])
            ins.args.push_back(varPool[distVar(gen)]);
            if (distProbability(gen) < 0.5) { // 50% chance to initialize with a value
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

            insList.push_back(ins); // Add FOR instruction

            // Generate inner instructions for the FOR loop
            std::uniform_int_distribution<int> distInnerBlockSize(1, 5); // 1 to 5 instructions inside loop
            int innerBlockSize = distInnerBlockSize(gen);

            for (int j = 0; j < innerBlockSize; j++) {
                Instruction innerIns;
                // Generate only computational/print/sleep ops inside FOR
                int innerOp_idx = distGeneralOp(gen);
                innerIns.opcode = opcode_pool[innerOp_idx];

                switch (innerIns.opcode) {
                case 1:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(std::to_string(distValue(gen)));
                    break;
                case 2:
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(varPool[distVar(gen)]);
                    innerIns.args.push_back(std::to_string(distSmallValue(gen)));
                    break;
                case 3:
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
            instructionsGenerated += (1 + innerBlockSize + 1); // FOR + inner + END
            continue; // Continue to next iteration of while loop to track total instructions
        }
        } // End of switch(opcode)

        insList.push_back(ins);
        instructionsGenerated++;
    } // End of while(instructionsGenerated < totalInstructions)

    // Final cleanup: If any FOR loops were opened but not closed (shouldn't happen with this logic), close them.
    // This is a safety fallback for generation, not a common path if logic is sound.
    while (currentDepth > 0) {
        Instruction endIns;
        endIns.opcode = 7;
        insList.push_back(endIns);
        currentDepth--;
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

    // Store the instruction index before execution, as execute() might change insCount_ for FOR/END jumps.
    size_t initialInsCount = insCount_;
    bool wasSleepingBeforeExecute = isSleeping_; // Capture state before execute

    // Execute the current instruction
    execute(insList[insCount_]);

    // Only advance insCount_ if execute() did NOT handle the pointer itself (FOR/END jumps)
    // AND if the process is NOT newly sleeping (handled by Core re-queueing and this method returning false next tick)
    if (insCount_ == initialInsCount) { // If execute did not explicitly change insCount_
        if (!isSleeping_) { // If the instruction didn't make the process sleep
            insCount_++; // Advance for normal instructions (DECLARE, ADD, SUBTRACT, PRINT)
        }
        else if (insList[initialInsCount].opcode == 5) { // If it was a SLEEP instruction and it made the process sleep
            insCount_++; // Always advance past the SLEEP instruction
        }
        // If insCount_ is initialInsCount and isSleeping_ is true, AND it's not a SLEEP opcode,
        // it means something unexpected set isSleeping_ or a bug exists. This should ideally not be hit.
    }

    // Check again if process finished after instruction execution
    if (insCount_ >= insList.size()) {
        finished_ = true;
        return false;
    }

    return true;
}