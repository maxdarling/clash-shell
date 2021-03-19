#include "src/Clash.h"
#include "src/loguru/loguru.hpp"
#include <vector>

int main(int argc, char *argv[]) {
    std::vector<std::string> args (argv, argv + argc);
    Clash::run(args);
}