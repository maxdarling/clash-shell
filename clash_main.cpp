#include "src/Clash.h"
#include <vector>

int main(int argc, char *argv[]) {
    std::vector<std::string> args (argv + 1, argv + argc);
    Clash::run(args);
}