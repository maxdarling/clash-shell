#include "src/Clash.h"
#include "src/loguru/loguru.hpp"
#include <vector>

int main(int argc, char *argv[]) {
/* logging */
    // loguru::init(argc, argv);
    // loguru::add_file("clash.log", loguru::Truncate, loguru::Verbosity_MAX);
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF; // disable logging

    std::vector<std::string> args (argv, argv + argc);
    Clash::run(args);
}