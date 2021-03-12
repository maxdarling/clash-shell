#include "Executor.h"
#include <iostream>

int main(int argc, char* argv[])
{
    loguru::init(argc, argv);
    loguru::add_file("clash.log", loguru::Truncate, loguru::Verbosity_MAX);

    LOG_F(INFO, "RUNNING EXECUTOR TESTS...");

    Executor e;
    std::cout << e.eval("x=abc| words.py 1\"$x\"2'$x'3`echo foo`\necho>foo abc| cat foo\nwords.py \"<\"'>'\\< `echo \\<`") << "\n";

    return 0;
}
