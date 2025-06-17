#include "Console.h"
#include "Process.h"
#include "Core.h"
#include "Scheduler.h"

/*
int main() {
    Console cli;
    cli.run();
    return 0;
}*/

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    Scheduler scheduler(2, "rr", 50);

    auto p1 = make_shared<Process>(1, "P1");
    auto p2 = make_shared<Process>(2, "P2");
    auto p3 = make_shared<Process>(3, "P3");

    p1->genRandInst();
    p2->genRandInst();
    p3->genRandInst();

    scheduler.submit(p1);
    scheduler.submit(p2);
    scheduler.submit(p3);

    scheduler.start();
    scheduler.waitUntilAllDone();

    cout << "All processes finished. Shutting down." << endl;

    return 0;
}




