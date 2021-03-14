#include "Executor.h"
#include "string_util.cpp"

using std::string;
using std::vector;

/**
 * Run a CLASH script.
 * 
 * @param input The CLASH script to be executed.
 */
string Executor::run(string input)
{
    vector<Command> commands;
    divide_into_commands(input, commands);
    for (Command &c : commands) eval_command(c);


    string output {};
    for (const Command &c : commands) {
        output += "[" + std::to_string(c.input_fd) + ", " 
            + std::to_string(c.output_fd) + "] " + c.bash_str + "\n";
    }

    return output;
}

/**
 * Divide a CLASH script into pipelines and commands.
 * 
 * As pipelines are detected, pipes are created, and the Command structs'
 * input_fd and output_fd fields are initialized with the ends of the pipes.
 * 
 * @param input A string containing clash script.
 * @param commands An empty vector, which will be populated with the commands.
 * 
 * @throws std::runtime_error If input contains unterminated command substitution
 * @throws std::runtime_error If input contains pipeline without both ends
 */
void Executor::divide_into_commands(string input, vector<Command> &commands)
{
    input += ';'; /* enforce input terminated by command separator */

    bool backslashed = false;
    bool double_quoted = false;
    bool single_quoted = false;
    bool command_sub = false;
    bool should_pipe = false;

    /* stores current accumulated command as we scan the input script */
    string cmd {};

    for (int i = 0; i < input.length(); i++) {
        switch (input[i]) {
            /* SPECIAL SYNTAX */
            case '\\':
                if (!single_quoted) backslashed = !backslashed;
                cmd += '\\';
                continue;
            case '\'':
                if (!backslashed && !double_quoted) {
                    single_quoted = !single_quoted;
                }
                break;
            case '"':
                if (!backslashed && !single_quoted) {
                    double_quoted = !double_quoted;
                }
                break;
            case '`':
                if (!backslashed && !single_quoted) {
                    command_sub = !command_sub;
                }
                break;
            /* COMMAND SEPARATORS */
            case '|':
            case ';':
            case '\n':
                if (!backslashed && !single_quoted && !double_quoted && 
                    !command_sub) {
                    /* add accumulated command to list and reset accumulator;
                     * empty commands are ignored (unless part of a pipeline) */
                    trim(cmd);
                    if (!cmd.empty()) commands.emplace_back(cmd);
                    else if (should_pipe) {
                        // TODO(ali): better error checking here
                        throw std::runtime_error("Incomplete pipeline");
                    }
                    else continue;
                    cmd = "";

                    /* CASE: set up pipeline from previous command to this one */
                    if (should_pipe) {
                        int pipe_fds[2];
                        pipe(pipe_fds);

                        commands.at(commands.size() - 2).output_fd = pipe_fds[1];
                        commands.at(commands.size() - 1).input_fd = pipe_fds[0];
                    }

                    should_pipe = input[i] == '|';
                }
                continue;
        }

        backslashed = false;
        cmd += input[i];
    }

    // TODO(ali): better error checking here
    if (single_quoted) {
        throw std::runtime_error("Unterminated single quotes");
    }
    if (double_quoted) {
        throw std::runtime_error("Unterminated double quotes");
    }
    if (command_sub) {
        throw std::runtime_error("Unterminated command substitution");
    }
    if (backslashed) {
        throw std::runtime_error("Backslash appears as last character of line");
    }
}

/**
 * Execute a CLASH command.
 * 
 * @param cmd The command to be executed.
 */
void Executor::eval_command(Command &cmd)
{

}

/**
 * Process the backslashes, double quotes, variable substitutions, and command
 * substitutions in a given CLASH command string.
 * 
 * The returned command is the result of processing these special syntax features
 * on the input command. The only special syntax feature present in the returned
 * command is single quotes, between which spaces do not delimit words.
 * 
 * @param cmd The raw CLASH command string to be processed.
 * @return The processed CLASH command string, which may contain single quotes.
 */
string Executor::process_special_syntax(const string &cmd)
{
    string processed_cmd {};

    /* FLAGS */
    bool backslashed = false;
    bool double_quoted = false;
    bool single_quoted = false;
    bool var_sub = false;
    bool command_sub = false;

    string var_name {};
    string sub_command {};
    for (int i = 0; i < cmd.length(); i++) {
        if (!backslashed) {
            /* set flag if a special character is encountered */
            switch (cmd[i]) {
                case '\\':
                    backslashed = true;
                    continue;
                case '"':
                    double_quoted = !double_quoted;
                    continue;
                case '\'':
                    single_quoted = !single_quoted;
                    continue;
                case '$':
                    var_sub = true;
                    continue;
                case '`':
                    command_sub = !command_sub;
                    continue;
            }
        } else backslashed = false;

        if (var_sub) {
            const string one_char_vars = "*#?";
            if (var_name.empty() && one_char_vars.find(cmd[i]) != string::npos) {
                // add cmd[i]
            }
            else if (!var_name.empty() && std::isdigit(var_name[0]) && std::isdigit(cmd[i])) {
                // add cmd[i]
            }
            else if (std::isalnum(cmd[i])) {
                // add cmd[i]
            }

            // if (std::isalnum(cmd[i]) || (var_name.empty() && (cmd[i] == '{' || cmd[i] == '')))

            // if (!var_name.empty()) {
            //     if (!std::isalnum(cmd[i]))
            // }
            // var_name += cmd[i];
        }

        processed_cmd += cmd[i];
    }

    return processed_cmd;
}

/**
 * Divide a CLASH command, which contains only single quotes as special 
 * characters, into words.
 * 
 * Backslashes, double quotes, variable substitutions, and command substitutions
 * in cmd should be processed before this method. The only special characters
 * considered here are spaces, which delimit words, and single quotes, which
 * neutralize the delimitting quality of spaces between them.
 * 
 * @param cmd A CLASH command which only contains single quotes as special
 *  characters, i.e. one that has been modified by process_special_syntax
 * @param words An empty vector, which will be populated with the words.
 */
void Executor::divide_into_words(string cmd, vector<string> &words)
{

}
