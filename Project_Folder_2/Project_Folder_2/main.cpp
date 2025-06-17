#include "Console.h"
#include "Process.h"
/*
int main() {
    Console cli;
    cli.run();
    return 0;
}*/

//int main() {
//    Process p(1, "TestProc");
//
//    // DECLARE x = 10
//    p.execute({ 1, {"x", "10"} });
//
//    // ADD y = x + 5 → y = 10 + 5 = 15
//    p.execute({ 2, {"y", "x", "5"} });
//
//    // SUBTRACT z = y - 3 → z = 15 - 3 = 12
//    p.execute({ 3, {"z", "y", "3"} });
//
//    // PRINT "Value of z is: " + z
//    p.execute({ 4, {"Value of z is: ", "z"} });
//
//    // SLEEP 2 ticks (should delay ~20ms)
//    p.execute({ 5, {"2"} });
//
//    // PRINT default message
//    p.execute({ 4, {} });
//
//    return 0;
//}

int main() {
    cout << "--- Manual Unit Test ---" << endl;
    Process p1(1, "ManualProc");

    p1.execute({ 1, {"x", "10"} });
    p1.execute({ 2, {"y", "x", "5"} });
    p1.execute({ 3, {"z", "y", "3"} });
    p1.execute({ 4, {"Value of z is: ", "z"} });
    p1.execute({ 5, {"2"} });
    p1.execute({ 4, {} });

    cout << "\n--- Full Instruction List Simulation ---" << endl;
    Process p2(2, "AutoProc");

    while (p2.runOneInstruction()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    cout << "AutoProc finished." << endl;

    return 0;
}
