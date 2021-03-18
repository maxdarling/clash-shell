#include "ExecutorTestHarness.h"
#include <iostream>

int main(int argc, char* argv[])
{
    loguru::init(argc, argv);
    loguru::add_file("clash.log", loguru::Truncate, loguru::Verbosity_MAX);

    LOG_F(INFO, "RUNNING EXECUTOR TESTS...");
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF; // don't write to stderr


    ExecutorTestHarness tests;
    /* 190 site spec tests */ 
    /*
    tests.add_test(
        "x=abc; words.py $x \"$x\" '$x' \"\\$x\"", 
            "$1: abc\n$2: abc\n$3: $x\n$4: $x\n");
    tests.add_test(
        "x=foo; echo file1 > zfoo.txt\ncat < z$x.txt\n", 
        "file1\n");
    // todo: add rest
    tests.add_test(
        "x=\\;; words.py \"a$x b; c|d\"", 
        "$1: a; b; c|d\n");
    */

    /* custom tests */
    // tests.add_test("x=abc; $x", "abc"); // try no newline...
    
    // // 2 ls in a row
    // tests.add_test("ls", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");  
    // tests.add_test("ls", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");  
    // // empty words.py
    // tests.add_test("words.py", "");

    // command sub:
    //tests.add_test("echo `echo dog`", "dog");
    tests.add_test("words.py `ls`", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");
    //tests.add_test("echo `ls`", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");
    tests.add_test("words.py pizza", "pizza");

    tests.run_all_tests();
}
