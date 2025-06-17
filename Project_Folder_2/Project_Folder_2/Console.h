#pragma once
#include <ctime>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <string>
#include "Scheduler.h"
#include "Screen.h"
#include "Process.h"

#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

//CONFIG STRUCT 

struct Config {
    int          num_cpu = 1;
    std::string  scheduler = "fcfs";
    uint64_t     quantum_cycles = 1;
    uint64_t     batch_process_freq = 1;
    uint64_t     min_ins = 1;
    uint64_t     max_ins = 1;
    uint64_t     delay_per_exec = 0;
};



class Console {
public:
    /* Entry‑point (blocking CLI loop) */
    void run() {
        clearScreen();
        string line;
        while (true) {
            cout << "csopesy> ";
            if (!getline(cin, line)) break;
            if (line == "exit") break;
            handleCommand(line);
        }
        cout << "Exiting...\n";
    }

private:

    void printHeader() {
        cout << " ,-----. ,---.   ,-----. ,------. ,------. ,---.,--.   ,--.  " << endl;
        cout << "'  .--./'   .-' '  .-.  '|  .--. '|  .---''   .-'\\  `.'  /  " << endl;
        cout << "|  |    `.  `-. |  | |  ||  '--' ||  `--, `.  `-. '.    /   " << endl;
        cout << "'  '--'\\.-'    |'  '-'  '|  | --' |  `---..-'    |  |  |    " << endl;
        cout << " `-----'`-----'  `-----' `--'     `------'`-----'   `--'     " << endl;
        cout << "\nCommand Line Interface\nType 'help' to see available commands\n";
    }
    void clearScreen() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        printHeader();
    }
    string getCurrentTimestamp() {
        time_t now = time(nullptr);
        tm localtm{};
#ifdef _WIN32
        localtime_s(&localtm, &now);
#else
        localtime_r(&now, &localtm);
#endif
        char buf[64];
        strftime(buf, sizeof(buf), "%m/%d/%Y, %I:%M:%S %p", &localtm);
        return string(buf);
    }


    void handleCommand(const string& line) {
        clearScreen();
        if (line == "help") {
            cout << "\nAvailable commands:" << endl;
            cout << "- initialize: initialize the specifications of the OS" << endl;
            cout << "- screen -ls: Show active and finished processes" << endl;
            cout << "- screen -s <process_name>: Create a new process" << endl;
            cout << "- screen -r <process_name>: Attach to a process screen" << endl;
            cout << "- scheduler-start: Start scheduler threads" << endl;
            cout << "- scheduler-stop: Stop scheduler threads" << endl;
            cout << "- report-util: Generate CPU utilization report" << endl;
            cout << "- clear: Clear the screen" << endl;
            cout << "- exit: Exit the program" << endl;
            cout << "Note: you must call initialize before any functional CLI command" << endl;
        }

        else if (line == "clear") { clearScreen(); return; }

        else if (line == "initialize" && initialized_ == false) {
            if (loadConfigFile("config.txt")) {
                initialized_ = true;
                cout << "\nLoaded configuration:\n"
                    << "  num-cpu            = " << cfg_.num_cpu << '\n'
                    << "  scheduler          = " << cfg_.scheduler << '\n'
                    << "  quantum-cycles     = " << cfg_.quantum_cycles << '\n'
                    << "  batch-process-freq = " << cfg_.batch_process_freq << '\n'
                    << "  min-ins            = " << cfg_.min_ins << '\n'
                    << "  max-ins            = " << cfg_.max_ins << '\n'
                    << "  delay-per-exec     = " << cfg_.delay_per_exec << '\n';
            }
            else {
                cout << "Initialization failed – check config.txt\n";
            }

            return;
        }
        else {
            if (initialized_ == true) {
                if (line == "screen -"); //check hw specs
                else if (line == "screen"); // check hw specs
                else if (line == "screen"); //check
                else if (line == "scheduler-start");
                else if (line == "scheduler-stop");
                else if (line == "report-util") {
                    generateReport();
                }
                else
                    cout << "[" << getCurrentTimestamp() << "] Unknown command: " << line << '\n';

            }
            else {
                cout << "Specifications have not yet been initialized!" << endl;
            }
        }


    }
    void generateReport() {
        /* Re‑use same info printed by screen -ls */
        ofstream out("csopesy-log.txt");
        if (!out) { cout << "Cannot create csopesy-log.txt\n"; return; }

        out << "CPU Cores : " << cfg_.num_cpu << '\n';
        out << "Running   : " << '\n';
        out << "Finished  : " << "\n\n";

        //for (const auto& p : runningProcs_)  out << p << "  (RUN)\n";
        //for (const auto& p : finishedProcs_) out << p << "  (FIN)\n";

        cout << "Report written to csopesy-log.txt\n";
    }
    //CONFIG LOADER 
    static string stripQuotes(string s) {
        if (!s.empty() && (s.front() == '\"' || s.front() == '\'')) s.erase(0, 1);
        if (!s.empty() && (s.back() == '\"' || s.back() == '\'')) s.pop_back();
        return s;
    }
    bool loadConfigFile(const string& path) {
        ifstream in(path);
        if (!in) { cout << "config.txt not found!\n"; return false; }

        unordered_map<string, string> kv;
        string k, v;
        while (in >> k >> v) kv[k] = stripQuotes(v);

        try {
            cfg_.num_cpu = stoi(kv.at("num-cpu"));
            cfg_.scheduler = kv.at("scheduler");
            cfg_.quantum_cycles = stoull(kv.at("quantum-cycles"));
            cfg_.batch_process_freq = stoull(kv.at("batch-process-freq"));
            cfg_.min_ins = stoull(kv.at("min-ins"));
            cfg_.max_ins = stoull(kv.at("max-ins"));
            cfg_.delay_per_exec = stoull(kv.at("delay-per-exec"));
        }
        catch (...) {
            cout << "Malformed config.txt – missing field\n";
            return false;
        }

        /* Basic range checks */
        if (cfg_.num_cpu < 1 || cfg_.num_cpu > 128) {
            cout << "num-cpu out of range (1–128)\n"; return false;
        }
        if (cfg_.scheduler != "fcfs" && cfg_.scheduler != "rr") {
            cout << "scheduler must be 'fcfs' or 'rr'\n"; return false;
        }
        return true;
    }


    Config cfg_;
    bool   initialized_ = false;
    std::unique_ptr<Scheduler> scheduler_;          // created after init
    std::vector<std::shared_ptr<Process>> processes_;
    std::unique_ptr<Screen> activeScreen_;          // one attached screen
};
