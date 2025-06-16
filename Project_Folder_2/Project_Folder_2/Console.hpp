#pragma once

#include <string>

class Console {
public:
    // Entry point for the CLI loop
    void run();

private:
    // ---------- UI helpers ----------
    void printHeader();
    void clearScreen();
    std::string getCurrentTimestamp();

    // ---------- Command dispatcher ----------
    void handleCommand(const std::string& line);
};
