#pragma once
#include "common.h"
#include "environment.h"
#include "jobs.h"
#include "builtins.h"
#include <string>
#include <vector>

class Shell;

/*  Executor: takes a parsed CmdList and runs it.
 *
 *  Handles:
 *    - fork/exec for external commands
 *    - Pipe chains (N commands → N-1 pipes)
 *    - I/O redirection (> >> < <<)
 *    - Background execution (&)
 *    - && and || short-circuit evaluation
 *    - Signal setup for child processes
 *    - Process group management (job control)
 */
class Executor {
public:
    Executor(Environment& env, JobTable& jobs, Builtins& builtins)
        : env_(env), jobs_(jobs), builtins_(builtins) {}

    // Execute a full command list; returns last exit status
    int execute(const CmdList& list, Shell& shell);

private:
    Environment& env_;
    JobTable&    jobs_;
    Builtins&    builtins_;

    int  exec_pipeline(const Pipeline& pl, Shell& shell);
    int  exec_simple(const SimpleCmd& cmd, int in_fd, int out_fd, Shell& shell);

    // Expand words in a SimpleCmd to argv strings
    std::vector<std::string> expand_argv(const SimpleCmd& cmd);

    // Apply redirections in the child process; returns false on error
    bool apply_redirects(const std::vector<Redirect>& redirs);

    // Setup signal dispositions for child process
    static void child_signals();

    // Build command string for job display
    static std::string cmd_string(const Pipeline& pl);

    int terminal_fd_{STDIN_FILENO};
};
