#pragma once
#include <iostream>
#include <memory>
#include <string>
#include "Process.h";
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
            cout << "Enter a command: ";
            if (!getline(cin, line)) break;
            if (line == "exit") break;
            handleCommand(line);
        }
        cout << "Returning to main menu...\n";
    }

private:
    shared_ptr<Process> process;

    // ----- helpers -----
    

    string processTag() const {
        //TODO: PROCESS NAME 
        return "proc";
    }

    void clearScreen() {
        #ifdef _WIN32
                system("cls");
        #else
                system("clear");
        #endif
        cout << "--- Process Screen --- (type 'exit' to leave)\n";
    }


    void handleCommand(const string& cmd) {
        if (cmd == "process-smi") {
            if (process) {
                cout << "[stub] process-smi info here\n"; // replace with real data
            }
            else {
                cout << "Demo process – no info available.\n";
            }
        }
        else {
            cout << "Unknown screen command.\n";
        }
    }
};
