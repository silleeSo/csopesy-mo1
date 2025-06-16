#pragma once
#include <ctime>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

/* ------------------------------------------------------------
   CONFIG STRUCT – stores parameters from config.txt
------------------------------------------------------------ */
struct Config {
    int          num_cpu = 1;       // [1,128]
    std::string  scheduler = "fcfs";  // "fcfs" | "rr"
    uint64_t     quantum_cycles = 1;       // RR slice
    uint64_t     batch_process_freq = 1;       // auto‑spawn freq
    uint64_t     min_ins = 1;       // min instructions
    uint64_t     max_ins = 1;       // max instructions
    uint64_t     delay_per_exec = 0;       // busy‑wait delay
};

/* ------------------------------------------------------------
   CONSOLE – header‑only implementation
------------------------------------------------------------ */
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
    /* ------------------- UI helpers ------------------- */
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

    /* ------------------- COMMAND DISPATCH ------------------- */
    void handleCommand(const string& line) {
        if (line == "help") {
            cout << "\nAvailable commands:\n"
                << "- initialize              : load config.txt\n"
                << "- clear                   : clear the screen\n"
                << "- exit                    : exit the program\n";
            return;
        }
        if (line == "clear") { clearScreen(); return; }

        if (line == "initialize") {
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

        cout << "[" << getCurrentTimestamp() << "] Unknown command: " << line << '\n';
    }

    /* ------------------- CONFIG LOADER ------------------- */
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

    /* ------------------- DATA ------------------- */
    Config cfg_;
    bool   initialized_ = false;
};
