#include "ExecutorTestHarness.h"
#include <iostream>

int main(int argc, char* argv[])
{
    loguru::init(argc, argv);
    loguru::add_file("clash.log", loguru::Truncate, loguru::Verbosity_MAX);

    LOG_F(INFO, "RUNNING EXECUTOR TESTS...");
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF; // don't write to stderr

    ExecutorTestHarness tests;


    /* SPEC TESTS */ 
    tests.add_test(
        "x=abc; words.py $x \"$x\" '$x' \"\\$x\"", 
            "$1: abc\n$2: abc\n$3: $x\n$4: $x\n");
    tests.add_test(
        "x=foo; echo file1 > zfoo.txt\ncat < z$x.txt\n", 
        "file1\n");

    // todo: fix variable assignment w/ quotes before these tests 
    // tests.add_test(
    //     "y='a\\nb'; words.py \\\"$y\\\"\n", 
    //     "$1: \"a\nb\"\n"
    // );
    // tests.add_test(
    //     "x='  a  b  '; words.py .$x.\n", 
    //     "$1: .\n$2: a\n$3: b\n$4: .\n"
    // );
    // tests.add_test(
    //     "x='  a  '; words.py $x$x\n", 
    //     "$1: a\n$2: b\n"
    // );
    // tests.add_test(
    //     "x='  a  b  '; words.py .\"$x\".", 
    //     "$1: .  a  b  .\n");
    // tests.add_test(
    //     "x=''; words.py $x $x", 
    //     "\n");
    tests.add_test(
        "words.py \"a `echo x y` \\$x\"", 
        "$1: a x y $x\n"); 
    // tests.add_test( // todo: fix variable assignment
    //     "x=\"\"; words.py \"\" $x\"\"", 
    //     "$1: \n$2: \n");
    tests.add_test(
        "x=abc; words.py '$x `echo z`'", 
        "$1: $x `echo z`\n"
    );
    tests.add_test(
        "words.py `echo a; echo b c`d", 
        "$1: a\n$2: b\n$3: cd\n"
    );
    tests.add_test(
        "x=abc; words.py 1\"$x\"2'$x'3`echo foo`",  // fails: are we doing too much word breaking?
        "$1: 1abc2$x3foo\n");
    tests.add_test(
        "echo>foo abc; cat foo", 
        "abc\n"
    );
    tests.add_test( // failing: we interpret the carats as io redirects when we shouldn't
        "words.py \"<\"'>'\\< `echo \\<`",
        "$1: <><\n$2: <\n"
    );
    tests.add_test(
        "x=\\;; words.py \"a$x b; c|d\"", 
        "$1: a; b; c|d\n"
    );


    /* CUSTOM TESTS */

    // tests.add_test("x=abc; $x", "abc"); // try no newline...
    
    // // 2 ls in a row
    // tests.add_test("ls", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");  
    // tests.add_test("ls", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");  
    // // empty words.py
    // tests.add_test("words.py", "");

    // command sub:
    //tests.add_test("echo `echo dog`", "dog");
    //tests.add_test("words.py `ls`", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");
    //tests.add_test("echo `ls`", "CMakeLists.txt  README.md       build           clash.log       clash_main.cpp  pp.cpp          src             test_commands   words.py");
    // tests.add_test("words.py", ""); // breaks: blocking on a read when there's no ouput
    // tests.add_test("echo pizza > trash_file; cat trash_file", "pizza\n");

    tests.run_all_tests();
}
