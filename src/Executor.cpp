#include "Executor.h"
#include "string_util.cpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream> // FOR DEBUGGING
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <filesystem>
// #include <sys/types.h>
// #include <sys/stat.h>
#include <fcntl.h>

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::cout, std::cerr, std::endl;

bool is_properly_formatted_var(const string& input);
std::unordered_set<std::string> extract_paths_from_PATH();

const static string kPATH_default = 
    "/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin";


/** 
 * Notes: 
 * 
 * - for whatever reason, 'pipe2' is not recognized on my (Max's) machine. 
 *   I really want to use this for 'O_CLOEXEC', but can't. For now, I'll just
 *   pollute the fd space, or whatever the consequence is ( .)_( .)  
 */


/**
 * Construct an Executor. Callers may optionally set argv to get special
 * argument variables like $0, $1, $2, $#, $*, and so on. 
 * 
 * @param argv the argv-style args a clash executable would've received, 
 *             in string/vector form. First arg should be executable name.
 */
 Executor::Executor(const vector<std::string>& argv) {
    _PATHs = extract_paths_from_PATH();

    // add custom variables
    _var_bindings["PATH"] = getenv("PATH");
    if (!argv.empty()) {
        _var_bindings["0"] = argv[0];
        int zero_idx = 0;
        if (argv.size() > 3 && argv[1] == "-c") {
            _var_bindings["0"] = argv[3];
            zero_idx = 3;
        } else if (argv.size() == 2) {
            _var_bindings["0"] = argv[1];
            zero_idx = 1;
        }
        for (int i = zero_idx + 1; i < argv.size(); ++i) {
            _var_bindings[std::to_string(i - zero_idx)] = argv[i];
        }
        _var_bindings["#"] = std::to_string(argv.size() - zero_idx - 1);
        string concat;
        for (const string& elem : argv) concat += elem + " ";
        _var_bindings["*"] = concat; 
    }
 }


/** 
 * Destructor for an executor instance. Exits with the exit status of the
 * most recently executed command. 
 */
 Executor::~Executor() {
     exit(std::stoi(_var_bindings["?"]));
 }

/**
 * Run a CLASH script.
 * 
 * @param input The CLASH script to be executed.
 */
void Executor::run(string input)
{
    vector<Command> commands;
    divide_into_commands(input, commands);

    vector<pid_t> pipeline_pids;
    for (Command &c : commands) eval_command(c, pipeline_pids);

    // pipelines only: wait for entire pipeline to finish, capture last 
    // command's status.  
    for (int i = 0; i < pipeline_pids.size(); ++i) {
        int status;
        waitpid(pipeline_pids[i], &status, 0);
        if (i == pipeline_pids.size() - 1) {
            _var_bindings["?"] = std::to_string(status);
        }
    }

}

/**
 * Divide a CLASH script into pipelines and commands.
 * 
 * As pipelines are detected, pipes are created, and the Command structs'
 * input_fd and output_fd fields are initialized with the ends of the pipes.
 * 
 * @param input A string containing clash script.
 * @param commands An empty vector, which will be populated with the commands.
 */
void Executor::divide_into_commands(string input, vector<Command> &commands)
{
    input += ';'; /* enforce input terminated by command separator */

    /* PARSING STATE FLAGS */
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
                if (backslashed || single_quoted || double_quoted || 
                    command_sub || var_name) break;
                /* add accumulated command to list and reset accumulator;
                 * empty commands are ignored (unless part of a pipeline) */
                trim(cmd);
                if (!cmd.empty()) commands.emplace_back(cmd);
                else if (should_pipe) {
                    throw ExecutorException("Incomplete pipeline");
                }
                else continue;
                cmd.clear();

                /* CASE: set up pipeline from previous command to this one */
                if (should_pipe) {
                    int pipe_fds[2];
                    pipe(pipe_fds);

                    commands.at(commands.size() - 2).output_fd = pipe_fds[1];
                    commands.at(commands.size() - 1).input_fd = pipe_fds[0];
                    commands.at(commands.size() - 2).is_part_of_pipeline = true;
                    commands.at(commands.size() - 1).is_part_of_pipeline = true;
                }

                should_pipe = (input[i] == '|');
                continue;
        }

        /* adding a normal character to the accumulating command */
        backslashed = false;
        cmd += input[i];
    }

    if (single_quoted) {
        throw ExecutorException("Unterminated single quotes");
    }
    if (double_quoted) {
        throw ExecutorException("Unterminated double quotes");
    }
    if (command_sub) {
        throw ExecutorException("Unterminated command substitution");
    }
    if (backslashed) {
        throw ExecutorException("Backslash appears as last character of line");
    }
    if (var_name) {
        throw ExecutorException("Unterminated braces for variable name");
    }
}

/**
 * Execute a CLASH command.
 * 
 * @param cmd The command to be executed.
 * @param pipeline_pids Output parameter to be populated with the pid of the
 *                      executed child process if 'cmd' is part of a pipeline. 
 */
void Executor::eval_command(Command &cmd, vector<pid_t>& pipeline_pids)
{
    cmd.bash_str = process_special_syntax(cmd.bash_str);
    vector<string> words;
    divide_into_words(cmd, words);
    if (words.empty()) return;

    // case #1: variable assignment
    if (words.size() == 1 && is_properly_formatted_var(words[0])) {
        int eq_idx = words[0].find('=');
        string var = words[0].substr(0, eq_idx);
        string val = words[0].substr(eq_idx + 1); 
        _var_bindings[var] = val; 
        LOG_F(INFO, "performed variable binding for %s : %s", var.c_str(), val.c_str());
    } 
    // case #2: builtin commands
    else if (words[0] == "cd") {
        try {
            fs::current_path((words[1]));
        }
        catch (...) {
            string msg = "cd: " + words[1] + ": " + strerror(errno);
            throw ExecutorException(msg); 
        }
    }
    else if (words[0] == "exit") {
        int status_code = 0;
        if (words.size() > 1) {
            try {
                status_code = std::stoi(words[1]); 
            }
            catch (...){
                string msg = "exit: " + words[1] + ": numeric argument required";
                throw ExecutorException(msg); 
            }
        }
        exit(status_code);
    }
    else if (words[0] == "export") {
        // export each existing var as an environment variable
        for (int i = 1; i < words.size(); ++i) {
            if (_var_bindings.count(words[i])) {
                int status = setenv(words[i].c_str(), _var_bindings[words[i]].c_str(), 1);
                if (status == -1) {
                    // bash behavior: do nothing for invalid variable
                    LOG_F(INFO, "export: invalid var: %s", words[i].c_str()); 
                }
            }
        }
    }
    else if (words[0] == "unset") {
        // delete each existing var (both in environment and bindings map)
        for (int i = 1; i < words.size(); ++i) {
            if (_var_bindings.count(words[i])) {
                int status = unsetenv(words[i].c_str());
                if (status == -1) {
                    // bash behavior: do nothing for invalid variable
                    LOG_F(INFO, "unset: invalid var: %s", words[i].c_str());
                }
                _var_bindings.erase(words[i]);
            }
        }
    }
    // case #3: executable
    else {
        string input_cmd = words[0];
        string complete_cmd;
        // case 1: path is specified explicitly
        if (input_cmd[0] == '/') {
            if (access(input_cmd.c_str(), X_OK) != 0) {
                throw ExecutorException(strerror(errno));
            }
            complete_cmd = input_cmd;
        }
        // case 2: check if the command is cached
        else if (_cached_command_paths.count(input_cmd)) {
            complete_cmd = _cached_command_paths[input_cmd];
            LOG_F(INFO, "cached path found: %s", complete_cmd.c_str());
        }
        // case 3: manually search PATH  
        else {
            for (const std::string& base_path : _PATHs) {
                string attempt_path = base_path + "/" + input_cmd;
                if (access(attempt_path.c_str(), X_OK) == 0) {
                    complete_cmd = attempt_path;
                    _cached_command_paths[input_cmd] = complete_cmd;
                    LOG_F(INFO, "full executable path found: %s", complete_cmd.c_str());
                    break;
                }
            }
        }
        // if we got here, we couldn't find a path to the executable. 
        if (complete_cmd == "") {
            throw ExecutorException("command not found: " + input_cmd);
        }

        /* execute command */
        pid_t pid = fork();
        if (pid == 0) {
            // setup i/o
            dup2(cmd.input_fd, STDIN_FILENO);
            dup2(cmd.output_fd, STDOUT_FILENO);
            // prepare argv
            vector<char *> argv (words.size() + 1);
            argv[0] = &complete_cmd[0];
            for (int i = 1; i < words.size(); ++i) {
                argv[i] = &words[i][0];
            }
            argv.back() = nullptr;

            execv(argv[0], argv.data());
        } 

        // parent: close pipes (stdin/out aren't pipes, so don't close em)
        if (cmd.input_fd != STDIN_FILENO) close(cmd.input_fd);
        if (cmd.output_fd != STDOUT_FILENO) close(cmd.output_fd);

        // parent: wait for child
        if (cmd.is_part_of_pipeline) {
            // pipelines run concurrently -> wait for this child later
            pipeline_pids.push_back(pid);
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) _var_bindings["?"] = std::to_string(WEXITSTATUS(status));
        }
    }
}

/**
 * Process backslashes, single quotes, double quotes, variable substitutions, 
 * and command substitutions for a given CLASH command string.
 * 
 * The returned command is the result of processing these special syntax features
 * on the input command. The two special syntax features present in the returned
 * command are single quotes and backslashes, which escape spaces/tabs that 
 * should not be treated as word separators, "<"/">" characters that should not 
 * be treated as I/O redirection operators, single quotes, and other backslashes.
 * 
 * @param cmd The raw CLASH command string to be processed.
 * 
 * @return The processed CLASH command string, in which single quotations may
 * be present and a backslash is an escape character.
 */
string Executor::process_special_syntax(const string &cmd)
{
    string processed_cmd {};

    /* PARSING STATE FLAGS */
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
                    throw ExecutorException("empty/invalid variable name");
                }

                // even if the var doesn't exist, this is correct (emtpy str)
                processed_cmd += _var_bindings[var_name];

                if (!exceeded_name_scope) continue;
            }
            /* CASE: still accumulating variable name, continue to cmd[i+1] */
            else continue;
        }

        /* ACCUMULATING SUBCOMMAND */
        if (command_sub) {
            if (cmd[i] == '`' && cmd[i - 1] != '\\') command_sub = false;
            else subcommand += cmd[i];

            /* CASE: subcommand accumulation ended in this iteration */
            if (!command_sub) {
                // run the subcommand and insert its output 
                string result = run_and_capture_output(subcommand);
                // remove trailing newline
                if (result.back() == '\n') result.pop_back();
                // word break on newlines and tabs
                std::replace(result.begin(), result.end(), '\n', ' ');
                std::replace(result.begin(), result.end(), '\t', ' ');
                processed_cmd += result;
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

    if (name_in_braces) {
        throw ExecutorException("unterminated braces for variable name");
    }
    if (single_quoted) {
        throw ExecutorException("unterminated single quotes");
    }
    if (double_quoted) {
        throw ExecutorException("unterminated double quotes");
    }
    if (command_sub) {
        throw ExecutorException("unterminated command substitution");
    }
    if (backslashed) {
        throw ExecutorException("backslash appears as last character of line");
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
 * (so that they do not delimit the current word), I/O redirection operators, 
 * single quotes, and other backslashes.
 * 
 * @param cmd A CLASH command which only contains single quotes and backslashes 
 * as special characters, i.e. one that has been modified by 
 * process_special_syntax().
 * @param words An empty vector, which will be populated with the words.
 */
void Executor::divide_into_words(Command &cmd, vector<string> &words)
{
    cmd.bash_str += ' ';
    const string &cmd_str = cmd.bash_str;

    /* PARSING STATE FLAGS */
    bool backslashed = false;
    bool single_quoted = false;
    bool double_quoted = false;
    bool quoted_word = false;
    bool i_redirect = false, o_redirect = false;

    /* stores current accumulated word as we scan the command string */
    string word {};

    for (int i = 0; i < cmd_str.length(); i++) {
        /* QUOTATION ENVIRONMENT */
        if (single_quoted || double_quoted) {
            if ((single_quoted && cmd_str[i] == '\'') || 
                (double_quoted && cmd_str[i] == '"')) {
                single_quoted = false;
                double_quoted = false;
            }
            else word += cmd_str[i];
            continue;
        }

        switch (cmd_str[i])
        {
            /* SPECIAL SYNTAX */
            case '\\':
                if (backslashed) word += '\\';
                backslashed = !backslashed;
                continue;
            case '\'':
            case '"':
                if (backslashed) {
                    word += cmd_str[i];
                    backslashed = false;
                    continue;
                }
                else {
                    quoted_word = true;
                    single_quoted = cmd_str[i] == '\'';
                    double_quoted = cmd_str[i] == '"';
                }
                continue;
            /* WORD SEPARATORS */
            case '<':
            case '>':
            case ' ':
            case '\t':
                if (backslashed) {
                    word += cmd_str[i];
                    backslashed = false;
                    continue;
                }

                /* CASE: a nonempty word has accumulated upon encountering a 
                 * word separator, or an empty word must be counted because it
                 * appears within quotes; process this word */
                if (!word.empty() || quoted_word) {
                    if (i_redirect) {
                        cmd.redirect_input(word);
                        i_redirect = false;

                    }
                    else if (o_redirect) {
                        cmd.redirect_output(word);
                        o_redirect = false;
                    }
                    else words.push_back(word);
                    word.clear();
                }
                /* CASE: two I/O redirection operators appear in a row */
                else if ((i_redirect || o_redirect) && 
                         (cmd_str[i] == '<' || cmd_str[i] == '>')) {
                    throw ExecutorException("missing redirection file name");
                }

                quoted_word = false;
                if (cmd_str[i] == '<') i_redirect = true;
                if (cmd_str[i] == '>') o_redirect = true;

                continue;
            default:
                backslashed = false;
                word += cmd_str[i];
                continue;
        }
    }

    if (i_redirect) {
        throw ExecutorException("missing input file name");
    }
    if (o_redirect) {
        throw ExecutorException("missing output file name");
    }
}


/**
 * Open a file and redirect the input of the command to its file descriptor.
 * 
 * @param fname Name of file to be opened.
 * 
 * @throws error If file does not exist
 */
void Executor::Command::redirect_input(const std::string &fname)
{
    int fd = open(fname.c_str(), O_RDONLY, 0644);
    if (fd == -1) {
        throw ExecutorException(strerror(errno));
    }
    input_fd = fd;
}

/**
 * Open a file and redirect the output of the command to its file descriptor.
 * 
 * @param fname Name of file to be opened. If it exists, the command output will
 * be appended to the file. If it does not exist, the file will be created.
 */
void Executor::Command::redirect_output(const std::string &fname)
{
    int fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644); 
    if (fd == -1) {
        throw ExecutorException(strerror(errno));
    }
    output_fd = fd;
}


/** 
 * Mimics exactly the behavior of the 'run' method, but captures its standard output 
 * and returns it as a string. 
 */
std::string Executor::run_and_capture_output(std::string input) {
    // 1: temporarily redirect stdout to a pipe
    int stdout_copy = dup(STDOUT_FILENO);
    int fds[2];
    pipe(fds);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);

    // 2: execute the command 
    try {
        Executor::run(input);
    }
    catch (...) {
        // restore stdout
        dup2(stdout_copy, STDOUT_FILENO);
        close(stdout_copy);
        // we should throw the thrown exception here b/c this function should 
        // exactly mimic 'run', only differing in fd setup + teardown. 
        throw;
    }

    // 3: read captured output and append it 
    string result;
    struct pollfd pfd {fds[0], POLL_IN, 0};
    // only attempt to read if there's data
    if (poll(&pfd, 1, 0) > 0) {
        char buf[512];
        int bytes_read = read(fds[0], buf, sizeof(buf));
        result = string(buf, bytes_read);
    }

    // 4: restore stdout
    dup2(stdout_copy, STDOUT_FILENO);
    close(stdout_copy);

    return result;
}


/**
 * Checks if the input string conforms to valid variable formatting. 
 * Rules:
 *  1. initial character is a letter followed by any number of letters or digits
 *  2. followed by a '='
 *  3. the rest is the variable's value, and has no format requirements
 */
 bool is_properly_formatted_var(const string& input) {
     if (input.size() < 2) return false;

     if (isalpha(input[0]) == 0) return false;

    int eq_idx = input.find('=');
    if (eq_idx == string::npos) return false;

    auto last = input.begin() + eq_idx;
    return last == find_if_not(input.begin() + 1, last, 
                               [] (unsigned char c) { return std::isalnum(c); });
 } 


/** 
 * Utility function. Parses and returns each path in PATH. 
 *
 * Note: the current directory '.' is always included, even in not in PATH.
 */ 
 std::unordered_set<std::string> extract_paths_from_PATH() {
    char *path_ptr = getenv("PATH");
    string PATH = path_ptr ? string(path_ptr) : kPATH_default;
    LOG_F(INFO, "existing PATH variable was %s", (path_ptr ? "found" : "not found"));
    int start_idx = 0;

    std::unordered_set<std::string> result;
    while(start_idx < PATH.length()) {
        int delim_idx = PATH.find(':', start_idx);
        result.insert(PATH.substr(start_idx, delim_idx - start_idx));
        if (delim_idx == string::npos) break;
        start_idx = delim_idx + 1;
    }

    result.insert(".");

    return result;
 }