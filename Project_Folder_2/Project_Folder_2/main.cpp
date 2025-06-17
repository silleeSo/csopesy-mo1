#include "Console.h"
#include "Process.h"
/*
int main() {
    Console cli;
    cli.run();
    return 0;
}*/

int main() {
    // Create a process with PID 1 and name "TestProc"
    Process p(1, "TestProc");

    // Loop through the generated instruction list and execute them
    for (const auto& ins : p.getInstructions()) {
        p.execute(ins);
    }

    return 0;
}