// Interactive command line for kvstore.
//
//   ./kvstore [data file]     (default: kvstore.log)
//
// Commands: put <key> <value>, get <key>, del <key>, compact, stats, help, quit
#include "kvstore.h"

#include <iostream>
#include <sstream>
#include <string>

static void printHelp() {
    std::cout << "commands:\n"
                 "  put <key> <value>   save a value (value can have spaces)\n"
                 "  get <key>           print a value\n"
                 "  del <key>           delete a key\n"
                 "  compact             rewrite the log with only live keys\n"
                 "  stats               show key count and log size\n"
                 "  quit                exit\n";
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "kvstore.log";
    KVStore db(path);
    std::cout << "kvstore v0.2 | " << path << " | " << db.size()
              << " keys loaded. type 'help' for commands.\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd, key;
        in >> cmd;

        if (cmd.empty()) {
            continue;
        } else if (cmd == "put") {
            std::string value;
            if (!(in >> key) || !std::getline(in >> std::ws, value)) {
                std::cout << "usage: put <key> <value>\n";
                continue;
            }
            db.put(key, value);
            std::cout << "OK\n";
        } else if (cmd == "get") {
            if (!(in >> key)) { std::cout << "usage: get <key>\n"; continue; }
            std::cout << db.get(key).value_or("(not found)") << "\n";
        } else if (cmd == "del") {
            if (!(in >> key)) { std::cout << "usage: del <key>\n"; continue; }
            std::cout << (db.del(key) ? "deleted" : "(not found)") << "\n";
        } else if (cmd == "compact") {
            auto before = db.logBytes();
            db.compact();
            std::cout << "log: " << before << " -> " << db.logBytes() << " bytes\n";
        } else if (cmd == "stats") {
            std::cout << db.size() << " keys, log is " << db.logBytes() << " bytes\n";
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else {
            std::cout << "unknown command '" << cmd << "'. type 'help'.\n";
        }
    }
    std::cout << "\n";
}
