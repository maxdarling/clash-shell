#include "Clash.h"
#include <stdexcept>
#include <iostream>
#include <fstream>

#include "Executor.h"

/*
Stuff encapsulated: 
-reading CL args
-the repl ('%' prompt)
-3 different operation modes (file, script, default)
-exception handling
*/

/*
 * Continually reads lines and evaluates them as commands until a stop 
 * condition is reached. 
 * 
 * @param file An open file from which to read
 * @param is_terminal 'true' if the file is a terminal, false otherwise. 
 */
void repl(std::istream& file, bool is_terminal) {
    Executor executor;
    while (true) {
        if (is_terminal) {
            std::cout << "% ";
        }
        std::string line;
        if (!getline(file, line)) {
            break;;
        }
        try {
            executor.run(line);
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    // determine why we broke from loop
    if (file.bad()) {
        std::cerr << "bad file" << std::endl;
    }
    else if (file.eof()) {
        std::cerr << "reached EOF" << std::endl;
    }
    else {
        std::cerr << "unknown error" << std::endl;
    }
}

void Clash::run(std::vector<std::string> args) {
    // case #1: input from stdin
    if (args.size() == 0) {
        // check if terminal or not
        bool is_terminal = (isatty(STDIN_FILENO) == 1);
        repl(std::cin, is_terminal);
    }
    // case #2: input from file
    else if (args.size() == 1) {
        // open file
        std::ifstream file(args[0]);
        if (file.fail()) {
            throw std::runtime_error("file not found: " + args[0]);
        } 
        repl(file, false);
    }
    // case #3: shell script
    else if (args.size() == 2 && args[0] == "-c") {
        // easy, just pass it to the executor.   
        Executor executor;
        try {
            executor.run(args[1]);
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }
    else {
        std::cerr << "clash: Invalid arguments" << std::endl;
    }
}