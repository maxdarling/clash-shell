#include "Executor.h"
#include <iostream>

class ExecutorTestHarness {
  public: 
    void add_test(std::string input, std::string expected_output) {
        _test_cases.push_back(TestCase{input, expected_output});
    }
    void run_all_tests() {
        int n_correct = 0;
        for (auto &[input, correct_output]: _test_cases) {
            std::string actual_output;
            try {
                actual_output = executor.run_and_capture_output(input);
            }
            catch (std::exception& e) {
                actual_output = e.what();
            }
            if (actual_output != correct_output) {
                std::cout << "Test FAILED: " << input << std::endl << 
                "got: " << std::endl << actual_output << std::endl << 
                "expected: " << std::endl << correct_output << std::endl; 
            }  
            else {
                std::cout << "Test PASSED: " << input << std::endl;
                // std::cout << "Test PASSED: " << input << std::endl << 
                // "got: " << std::endl << actual_output << std::endl << 
                // "expected: " << std::endl << correct_output << std::endl; 
                ++n_correct;
            }
        }
        std::cout << std::endl << std::endl << "TOTAL: " << n_correct << " / " << 
        _test_cases.size() << " tests passed." << std::endl;
    }
  
  private:
    Executor executor;

    struct TestCase {
        std::string input, correct_output;
    };
    std::vector<TestCase> _test_cases;
};