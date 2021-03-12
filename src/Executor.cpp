#include "Executor.h"
#include "string_util.cpp"

using std::string;
using std::vector;

string Executor::eval(string input)
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
    int delimiter_idx;
    bool should_pipe = false;
    while ((delimiter_idx = input.find_first_of("\n;|")) != string::npos) {
        /* parse command to the left of delimitter */
        string lcmd = input.substr(0, delimiter_idx);
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
        should_pipe = input[delimiter_idx] == '|';

        input = input.substr(delimiter_idx + 1);
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
    /* CASE: input missing the receiving of a pipeline */
    else if (should_pipe) {
        // TODO(ali): throw something better here
        throw std::runtime_error("missing receiving end of pipeline");
    }
}
