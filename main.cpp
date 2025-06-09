#include "database.h"
#include <iostream>
#include <fmt/core.h>

int main() {
    Database db;
    std::string line;
    fmt::print("Enter queries below:\n");
    while (std::getline(std::cin, line)) {
        try {
            db.execute(line);
        } catch (const std::exception& e) {
            fmt::print("Error: {}\n", e.what());
        }
    }
}
