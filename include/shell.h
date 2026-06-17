#pragma once
#include "environment.h"
#include "jobs.h"
#include "history.h"
#include "builtins.h"
#include "executor.h"
#include <string>
#include <atomic>

/*  Shell: the top-level read-eval-print loop.
 *
 *  Ties together: Lexer → Parser → Executor
 *  Also owns:     Environment, JobTable, History, Builtins
 *
 *  Handles:
 *    - Interactive prompt with $? status indicator
 *    - SIGINT (Ctrl-C): abort current line
 *    - SIGTSTP (Ctrl-Z): suspend foreground job
 *    - SIGCHLD: reap background children
 *    - Startup files (~/.minibashrc)
 *    - Script mode (non-interactive, read from file)
 */
class Shell {
public:
    Shell();
    ~Shell();

    // Run a single line (used by source builtin and tests)
    int run_line(const std::string& line);

    // Interactive REPL
    void run_interactive();

    // Script mode: read from file
    int run_script(const std::string& path);

    // Prompt string (PS1-style)
    std::string prompt() const;

private:
    Environment env_;
    JobTable    jobs_;
    History     hist_;
    Builtins    builtins_;
    Executor    exec_;

    bool interactive_{true};

    void setup_signals();
    void load_rc();

    // Read a line (with readline if available, else getline)
    std::string read_line(bool& eof);
};
