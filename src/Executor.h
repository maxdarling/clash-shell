#include "loguru/loguru.hpp"
#include <unistd.h> // for STDIN_FILENO, STDOUT_FILENO
#include <unordered_map>
#include <vector>
#include <string>


class Executor {
  public:
    Executor() {}
    std::string run(std::string input);

  private:
    struct Command {
        Command(std::string cmd) 
          : bash_str(cmd), input_fd(STDIN_FILENO), output_fd(STDOUT_FILENO) {}

        std::string bash_str;
        int input_fd;
        int output_fd;
    };

    std::unordered_map<std::string, std::string> var_bindings;

    void divide_into_commands(std::string input, std::vector<Command> &commands);
    void eval_command(Command &cmd);
    std::string process_special_syntax(const std::string &cmd);
    void divide_into_words(std::string cmd, std::vector<std::string> &words);
};
