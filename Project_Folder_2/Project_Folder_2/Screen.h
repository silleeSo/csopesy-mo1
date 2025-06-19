// Screen.h
#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <vector> // For logs
#include <unordered_map> // For variables
#include "Process.h"
#include "GlobalState.h" // For globalCpuTicks in process-smi display
using namespace std;
/*
    SCREEN OVERVIEW
    - wraps process basically
    - has the ui features of a process
*/

class Screen {
public:
    Screen(shared_ptr<Process> proc) : process{ proc } {}

    // Enters the process screen loop.
    void run() {
        clearScreen();
        string line;
        while (true) {
            cout << process->getName() << ":> "; // Show process name in prompt
            if (!getline(cin, line)) break;
            if (line == "exit") break;
            handleCommand(line);
        }
        cout << "Returning to main menu...\n";
    }

private:
    shared_ptr<Process> process;

    // ----- helpers -----

    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        cout << "--- Process Screen for " << process->getName() << " (PID: " << process->getPid() << ") --- (type 'exit' to leave)\n";
        cout << "Current Global CPU Tick: " << globalCpuTicks.load() << "\n\n";
    }


    void handleCommand(const string& cmd) {
        clearScreen(); // Clear screen on each command to refresh view

        if (cmd == "process-smi") {
            if (process) {
                cout << process->smi() << endl; // Use the detailed smi method from Process
            }
            else {
                cout << "Error: No process attached to this screen.\n";
            }
        }
        else {
            cout << "Unknown screen command.\n";
        }
    }
};