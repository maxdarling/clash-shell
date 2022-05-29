#include "loguru/loguru.hpp"
#include <unistd.h> // for STDIN_FILENO, STDOUT_FILENO
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>


/**
 * An Executor instance implements a clash shell session and provides a simple 
 * interface with which to parse and execute command-line commands. Internal 
 * session state like (environment) variables, the working directory, etc. are
 * also managed by Executor.
 * 
 * The Executor class therefore implements the bulk of clash, and leaves
 * topmost-level functionality like exception handling, command-line argument
 * parsing, and execution modes (terminal, file, or script) to the Clash class.  
 * 
 * Exceptions: This class throws ExecutorException's in the case of malformed 
 * commands.  
 */  
class Executor {
  public:
    Executor(const std::vector<std::string>& argv = {});
    ~Executor();
    void execute_command(std::string input);
    std::string execute_command_and_capture_output(std::string input);

  private:
    struct Command {
        Command(std::string cmd) 
          : bash_str(cmd), input_fd(STDIN_FILENO), output_fd(STDOUT_FILENO) {}
        void redirect_input(const std::string &fname);
        void redirect_output(const std::string &fname);

        std::string bash_str;
        int input_fd;
        int output_fd;
        bool is_part_of_pipeline = false;
    };

    std::unordered_map<std::string, std::string> _var_bindings;
    std::unordered_map<std::string, std::string> _cached_command_paths;
    std::unordered_set<std::string> _PATHs;

    void divide_into_commands(std::string input, 
                              std::vector<Command> &commands);
    void eval_command(Command &cmd, std::vector<pid_t>& pipeline_pids);
    std::string process_special_syntax(const std::string &cmd);
    void divide_into_words(Command &cmd, std::vector<std::string> &words);


  public: 
    // custom exception class
    class ExecutorException: public std::exception {
        private:
            std::string _msg;
        public:
            ExecutorException(const std::string& msg) : _msg(msg){}

            virtual const char* what() const noexcept override
            {
                return _msg.c_str();
            } 
        };
};
