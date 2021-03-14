#include "Executor.h"
#include "string_util.cpp"
#include <iostream> // FOR DEBUGGING

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

    for (Command &c : commands) {
        std::cerr << "[" + std::to_string(c.input_fd) + ", " + std::to_string(c.output_fd) + "] " + c.bash_str + "\n";
        eval_command(c);
    }

    return {};
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
    bool var_name = false;
    bool should_pipe = false;

    /* stores current accumulated command as we scan the input script */
    string cmd {};

    for (int i = 0; i < input.length(); i++) {
        switch (input[i]) {
            case '\\':
                if (!single_quoted && !var_name) backslashed = !backslashed;
                cmd += '\\';
                continue;
            case '\'':
                if (!backslashed && !double_quoted && !var_name) {
                    single_quoted = !single_quoted;
                }
                break;
            case '"':
                if (!backslashed && !single_quoted && !var_name) {
                    double_quoted = !double_quoted;
                }
                break;
            case '`':
                if (!backslashed && !single_quoted && !var_name) {
                    command_sub = !command_sub;
                }
                break;
            case '$':
                if (!backslashed && !single_quoted && !var_name && 
                    input[i+1] == '{') { var_name = true; }
                break;
            case '}':
                var_name = false;
                break;
            /* COMMAND SEPARATORS */
            case '|':
            case ';':
            case '\n':
                if (!backslashed && !single_quoted && !double_quoted && 
                    !command_sub && !var_name) {
                    /* add accumulated command to list and reset accumulator;
                     * empty commands are ignored (unless part of a pipeline) */
                    trim(cmd);
                    if (!cmd.empty()) commands.emplace_back(cmd);
                    else if (should_pipe) {
                        // TODO(ali): better error checking here
                        throw std::runtime_error("Incomplete pipeline");
                    }
                    else continue;
                    cmd.clear();

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
    if (var_name) {
        throw std::runtime_error("Unterminated braces for variable name");
    }
}

/**
 * Execute a CLASH command.
 * 
 * @param cmd The command to be executed.
 */
void Executor::eval_command(Command &cmd)
{
    string processed_cmd = process_special_syntax(cmd.bash_str);
    std::cerr << processed_cmd << "\n";
}

/**
 * Process backslashes, single quotes, double quotes, variable substitutions, 
 * and command substitutions for a given CLASH command string.
 * 
 * The returned command is the result of processing these special syntax features
 * on the input command. The only special syntax features present in the returned
 * command are single quotes and backslashes, which escape spaces/tabs that 
 * should not be treated as word separators, "<"/">" characters that should not 
 * be treated as I/O redirections, single quotes, and other backslashes.
 * 
 * @param cmd The raw CLASH command string to be processed.
 * @return The processed CLASH command string, in which single quotations may
 * be present and a backslash is an escape character.
 */
string Executor::process_special_syntax(const string &cmd)
{
    string processed_cmd {};

    bool backslashed = false;
    bool double_quoted = false;
    bool single_quoted = false;
    bool var_sub = false, name_in_braces = false;
    bool command_sub = false;

    string var_name {};
    string subcommand {};

    for (int i = 0; i < cmd.length(); i++) {
        /* SINGLE QUOTATION ENVIRONMENT */
        if (single_quoted) {
            processed_cmd += cmd[i];
            if (cmd[i] == '\'') single_quoted = false;
            continue;
        }

        /* ACCUMULATING VARIABLE NAME */
        if (var_sub) {
            static const string ONE_CHAR_VARS = "#*?";
            /* set to true if we should switch on cmd[i] this iteration */
            bool exceeded_name_scope = false;
            /* CASE: process first character of variable name */
            if (var_name.empty()) {
                if (ONE_CHAR_VARS.find(cmd[i]) != string::npos) {
                    var_name += cmd[i];
                    var_sub = false;
                }
                else if (cmd[i] == '{') name_in_braces = true;
                else if (std::isalnum(cmd[i])) var_name += cmd[i];
                else var_sub = false;
            }
            else if (name_in_braces) {
                if (cmd[i] == '}') {
                    name_in_braces = false;
                    var_sub = false;
                }
                else var_name += cmd[i];
            }
            /* CASE: if a non-alphanumeric character is encountered, or if the 
             * variable name is supposed to be a digit and a non-digit character
             * is enounctered, end the variable name accumulation */
            else if (!std::isalnum(cmd[i]) ||
                    (std::isdigit(var_name[0]) && !std::isdigit(cmd[i]))) {
                var_sub = false;
                exceeded_name_scope = true;
            }
            else var_name += cmd[i];

            /* CASE: variable name accumulation ends in this iteration */
            if (!var_sub || i == cmd.length() - 1) {
                if (var_name.empty()) {
                    // TODO(ali): better error checking
                    throw std::invalid_argument("empty/invalid variable name");
                }

                // TODO(ali): always append var_bindings[var_name] to processed_cmd
                // i.e. processed_cmd += var_bindings[var_name];
                processed_cmd += "[binding of \"" + var_name + "\"]";

                if (!exceeded_name_scope) continue;
            }
            /* CASE: still accumulating variable name, continue to cmd[i+1] */
            else continue;
        }

        /* ACCUMULATING SUBCOMMAND */
        if (command_sub) {
            if (cmd[i] == '`') command_sub = false;
            else subcommand += cmd[i];

            /* CASE: subcommand accumulation ended in this iteration */
            if (!command_sub) {
                // TODO(ali): execute subcommand, capturing its output, append output to processed_cmd
                processed_cmd += "[output of \"" + subcommand + "\"]";
            }

            continue;  
        }

        /* SPECIAL SYNTAX */
        switch (cmd[i]) {
            case '\\':
                if (backslashed) processed_cmd += '\\';
                backslashed = !backslashed;
                continue;
            case '"':
                if (backslashed) {
                    processed_cmd += '"';
                    backslashed = false;
                }
                else {
                    /* transform double quote environments into single quotes */
                    processed_cmd += '\'';
                    double_quoted = !double_quoted;
                }
                continue;
            case '\'':
                if (double_quoted) processed_cmd += '\\';
                processed_cmd += '\'';
                if (backslashed || double_quoted) backslashed = false;
                /* begin single quotation environment */
                else single_quoted = true;
                continue;
            case '$':
                if (backslashed) {
                    processed_cmd += '$';
                    backslashed = false;
                }
                /* begin accumulating variable name */
                else {
                    var_sub = true;
                    name_in_braces = false;
                    var_name.clear();
                }
                continue;
            case '`':
                if (backslashed) {
                    processed_cmd += '`';
                    backslashed = false;
                }
                /* begin accumulating subcommand */
                else {
                    command_sub = true;
                    subcommand.clear();
                }
                continue;
            /* word separation and redirection are handled later, so preserve
             * their escaping */
            case ' ':
            case '\t':
            case '<':
            case '>':
                if (backslashed && !double_quoted) processed_cmd += '\\';
            default:
                backslashed = false;
                processed_cmd += cmd[i];
                break;
        }
    }

    // TODO(ali): better error checking
    if (name_in_braces) {
        throw std::invalid_argument("unterminated braces for variable name");
    }
    if (single_quoted) {
        throw std::runtime_error("unterminated single quotes");
    }
    if (double_quoted) {
        throw std::runtime_error("unterminated double quotes");
    }
    if (command_sub) {
        throw std::runtime_error("unterminated command substitution");
    }
    if (backslashed) {
        throw std::runtime_error("backslash appears as last character of line");
    }

    return processed_cmd;
}

/**
 * Divide a CLASH command, which contains only single quotations and the 
 * backslash as an escape character, into words.
 * 
 * Double quotes, variable substitutions, and command substitutions should be
 * processed before this method. The only special characters considered here are
 * single quotations and the backslash, which is used to escape space characters 
 * (so that they do not delimit the current word), I/O redirections, single 
 * quotes, and other backslashes.
 * 
 * @param cmd A CLASH command which only contains single quotes and backslashes 
 * as special characters, i.e. one that has been modified by 
 * process_special_syntax().
 * @param words An empty vector, which will be populated with the words.
 */
void Executor::divide_into_words(string cmd, vector<string> &words)
{

}
