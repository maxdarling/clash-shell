#include "loguru/loguru.hpp"
#include <unistd.h> // for STDIN_FILENO, STDOUT_FILENO
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
//#include "ExecutorTestHarness.h"


class Executor {
  public:
    Executor() {}
    void run(std::string input);

  private:
    struct Command {
        Command(std::string cmd) 
          : bash_str(cmd), input_fd(STDIN_FILENO), output_fd(STDOUT_FILENO) {}
        void redirect_input(const std::string &fname);
        void redirect_output(const std::string &fname);
        // TODO(ali): destructor that closes files?

        std::string bash_str;
        int input_fd;
        int output_fd;
    };

    std::unordered_map<std::string, std::string> _var_bindings;
    std::unordered_map<std::string, std::string> _cached_command_paths;

    void divide_into_commands(std::string input, std::vector<Command> &commands);
    void eval_command(Command &cmd);
    std::string process_special_syntax(const std::string &cmd);
    void divide_into_words(Command &cmd, std::vector<std::string> &words);

    // convenience:
    std::string run_and_capture_output(std::string input);

    friend class ExecutorTestHarness;
};
