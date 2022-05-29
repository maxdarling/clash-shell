#include "Clash.h"
#include <stdexcept>
#include <iostream>
#include <fstream>

#include "Executor.h"

/*
 * Continually reads lines and evaluates them as commands until a stop 
 * condition is reached. 
 * 
 * @param file An open file from which to read
 * @param is_terminal 'true' if the file is a terminal, false otherwise. 
 */
void repl(std::istream& file, bool is_terminal, Executor& executor) {
    while (true) {
        if (is_terminal) {
            std::cout << "% ";
        }
        std::string line;
        if (!getline(file, line)) {
            break;;
        }
        try {
            executor.execute_command(line);
        }
        catch (Executor::ExecutorException& e) {
            std::cerr << "clash: " << e.what() << std::endl;
        }
        catch (std::exception& e) {
            std::cerr << "clash: " << e.what() << std::endl;
            LOG_F(INFO, "uncaught exception: %s", e.what());
        }
    }

    // determine why we broke from loop
    if (file.bad()) {
        std::cerr << "clash: bad file: " << strerror(errno) << std::endl;
    }
    else if (!file.eof()) {
        std::cerr << "clash: unknown error: " << strerror(errno) << std::endl;
    }
}

void Clash::run(std::vector<std::string> args) {
    Executor executor(args);
    // case #1: input from stdin
    if (args.size() == 1) {
        bool is_terminal = (isatty(STDIN_FILENO) == 1);
        repl(std::cin, is_terminal, executor);
    }
    // case #2: input from file
    else if (args.size() == 2) {
        std::ifstream file(args[1]);
        if(file.fail()) {
            std::cerr << "clash: " << strerror(errno) << std::endl;
            return;
        }
        repl(file, false, executor);
    }
    // case #3: shell script
    else if (args.size() == 3 && args[1] == "-c") {
        try {
            executor.execute_command(args[1]);
        }
        catch (Executor::ExecutorException& e) {
            std::cerr << "clash: " << e.what() << std::endl;
        }
        catch (std::exception& e) {
            std::cerr << "clash: " << e.what() << std::endl;
            LOG_F(INFO, "uncaught exception: %s", e.what());
        }
    }
    else {
        std::cerr << "clash: Invalid arguments" << std::endl;
    }
}