#include "Executor.h"
#include "string_util.cpp"

using std::string;
using std::vector;

string Executor::run(string input)
{
    vector<Command> commands;
    divide_into_commands(input, commands);

    string output {};
    for (const Command &c : commands) {
        output += c.bash_str + " <" + std::to_string(c.input_fd) 
               + ", " + std::to_string(c.output_fd) + ">\n";
    }

    return output;
}

/**
 * Divide a CLASH script into pipelines and commands.
 * 
 * As pipelines are detected, pipes are created, and the Command structs'
 * input_fd and output_fd fields are initialized accordingly.
 * 
 * @param input A string containing clash script.
 * @param commands An empty vector, which will be populated with the commands.
 */
void Executor::divide_into_commands(string input, vector<Command> &commands)
{
    const string separators = "`\n;|";
    int separator_idx;
    bool should_pipe = false;
    while ((separator_idx = input.find_first_of(separators)) != string::npos) {
        /* CASE: skip over command substitutions; these are handled during the 
         * evaluation of the individual commands */
        while (input[separator_idx] == '`') {
            separator_idx = input.find("`", separator_idx + 1);
            if (separator_idx == string::npos) {
                // TODO(ali): throw something better here
                throw std::runtime_error("unclosed command substitution");    
            }
            separator_idx = input.find_first_of(separators, separator_idx + 1);
        }
        if (separator_idx == string::npos) break;

        /* parse command to the left of separator */
        string lcmd = input.substr(0, separator_idx);
        trim(lcmd);
        commands.emplace_back(lcmd);

        /* CASE: set up pipeline from previous command to this one */
        if (should_pipe) {
            int pipe_fds[2];
            pipe(pipe_fds);

            commands.at(commands.size() - 2).output_fd = pipe_fds[1];
            commands.at(commands.size() - 1).input_fd = pipe_fds[0];

            should_pipe = false;
        }
        should_pipe = input[separator_idx] == '|';

        input = input.substr(separator_idx + 1);
    }

    /* input may contain a final command */
    trim(input);
    if (!input.empty()) {
        commands.emplace_back(input);
        if (should_pipe) {
            int pipe_fds[2];
            pipe(pipe_fds);

            commands.at(commands.size() - 2).output_fd = pipe_fds[1];
            commands.at(commands.size() - 1).input_fd = pipe_fds[0]; 
        }
    }
    else if (should_pipe) {
        // TODO(ali): throw something better here
        throw std::runtime_error("missing receiving end of pipeline");
    }
}

/**
 * Execute a CLASH command.
 * 
 * @param cmd The command to be executed.
 */
void Executor::eval_command(Command cmd)
{

}