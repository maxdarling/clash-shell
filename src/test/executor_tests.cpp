#include "ExecutorTestHarness.h"
#include "../loguru/loguru.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    /* logging */
    // loguru::init(argc, argv);
    // loguru::add_file("clash.log", loguru::Truncate, loguru::Verbosity_MAX);
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF; // disable logging

    ExecutorTestHarness tests;

    /* SPEC TESTS */ 
    tests.add_test(
        "x=abc; words.py $x \"$x\" '$x' \"\\$x\"", 
            "$1: abc\n$2: abc\n$3: $x\n$4: $x\n");
    tests.add_test(
        "x=foo; echo file1 > zfoo.txt\ncat < z$x.txt\n", 
        "file1\n");
    // problem: words.py does not render newlines
    // tests.add_test(
    //     "y='a\\nb'; words.py \\\"$y\\\"\n", 
    //     "$1: \"a\nb\"\n"
    // );
    tests.add_test(
        "x='  a  b  '; words.py .$x.\n", 
        "$1: .\n$2: a\n$3: b\n$4: .\n"
    );
    tests.add_test(
        "x='  a  '; words.py $x$x\n", 
        "$1: a\n$2: a\n"
    );
    tests.add_test(
        "x='  a  b  '; words.py .\"$x\".", 
        "$1: .  a  b  .\n");
    // problem: words.py does not render newlines
    // tests.add_test(
    //     "x=''; words.py $x $x", 
    //     "\n");
    tests.add_test(
        "words.py \"a `echo x y` \\$x\"", 
        "$1: a x y $x\n"); 
    tests.add_test(
        "x=\"\"; words.py \"\" $x\"\"", 
        "$1: \n$2: \n");
    tests.add_test(
        "x=abc; words.py '$x `echo z`'", 
        "$1: $x `echo z`\n"
    );
    tests.add_test(
        "words.py `echo a; echo b c`d", 
        "$1: a\n$2: b\n$3: cd\n"
    );
    tests.add_test(
        "x=abc; words.py 1\"$x\"2'$x'3`echo foo`",
        "$1: 1abc2$x3foo\n");
    tests.add_test(
        "echo>foo abc; cat foo", 
        "abc\n"
    );
    // problem: Why is the final '<' considered escaped? we don't know
    // tests.add_test( 
    //     "words.py \"<\"'>'\\< `echo \\<`",
    //     "$1: <><\n$2: <\n"
    // );
    tests.add_test(
        "x=\\;; words.py \"a$x b; c|d\"", 
        "$1: a; b; c|d\n"
    );


    /* CUSTOM TESTS */
    // blank input
    tests.add_test("", "");
    tests.add_test("words.py", "");

    // file redirection
    tests.add_test("echo pizza > trash_file; cat trash_file", "pizza\n");
    tests.add_test("cat < trash_file", "pizza\n");
    tests.add_test("cat < fakefile", "No such file or directory");

    // concurrent piping
    tests.add_test("echo 'this should take 1s, not 10s';"
                   "sleep 1 | sleep 1 | sleep 1 | sleep 1 | sleep 1 | "
                   "sleep 1 | sleep 1 | sleep 1 | sleep 1 | sleep 1", 
                   "this should take 1s, not 10s\n");

    // built-ins error handling
    tests.add_test("cd fakedirectory", 
                   "cd: fakedirectory: No such file or directory");
    tests.add_test("exit fakestatus", 
                   "exit: fakestatus: numeric argument required");
    tests.add_test("export fakevar", "");
    tests.add_test("unset fakevar", "");

    tests.run_all_tests();
}
