#pragma once
#include <ctime>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;
/*
    - config.txt
    - report-util
*/
class Console {
public:
    // Entry point for the CLI loop
    //can split into a thread?
    void run() {
        clearScreen();
        std::string line;
        while (true) {
            std::cout << "csopesy> ";
            if (!std::getline(std::cin, line)) break;
            if (line == "exit") break;
            handleCommand(line);
        }
        std::cout << "Exiting...\n";
    }

private:
    // ---------- UI helpers ----------
    void printHeader() {
        std::cout << " ,-----. ,---.   ,-----. ,------. ,------. ,---.,--.   ,--.  " << std::endl;
        std::cout << "'  .--./'   .-' '  .-.  '|  .--. '|  .---''   .-'\\  `.'  /  " << std::endl;
        std::cout << "|  |    `.  `-. |  | |  ||  '--' ||  `--, `.  `-. '.    /   " << std::endl;
        std::cout << "'  '--'\\.-'    |'  '-'  '|  | --' |  `---..-'    |  |  |    " << std::endl;
        std::cout << " `-----'`-----'  `-----' `--'     `------'`-----'   `--'     " << std::endl;
        std::cout << "\nCommand Line Interface" << std::endl;
        std::cout << "Type 'help' to see available commands" << std::endl;
    }

    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        printHeader();
    }

    std::string getCurrentTimestamp() {
        std::time_t now = std::time(nullptr);
        std::tm localtm{};
#ifdef _WIN32
        localtime_s(&localtm, &now);
#else
        localtime_r(&now, &localtm);
#endif
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%m/%d/%Y, %I:%M:%S %p", &localtm);
        return std::string(buffer);
    }

    // ---------- Command dispatcher ----------
    void handleCommand(const std::string& line) {
        if (line == "help") {
            cout << "\nAvailable commands:" << endl;
            cout << "- screen -ls: Show active and finished processes" << endl;
            cout << "- screen -s <process_name>: Create a new process" << endl;
            cout << "- screen -r <process_name>: Attach to a process screen" << endl;
            cout << "- scheduler-start: Start scheduler threads" << endl;
            cout << "- scheduler-stop: Stop scheduler threads" << endl;
            cout << "- clear: Clear the screen" << endl;
            cout << "- exit: Exit the program" << endl;
        }

        else {
            std::cout << "[" << getCurrentTimestamp() << "] Unknown command: " << line << "\n";
        }
    }
};