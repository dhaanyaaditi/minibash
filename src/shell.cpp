#include "shell.h"
#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// Global flag set by SIGINT
static volatile sig_atomic_t g_interrupted = 0;

static void sigint_handler(int) {
    g_interrupted = 1;
    write(STDOUT_FILENO, "\n", 1);
}

static void sigchld_handler(int) {
    // Reaping done in JobTable::reap() called from REPL
}

Shell::Shell()
    : hist_(1000),
      builtins_(env_, jobs_, hist_),
      exec_(env_, jobs_, builtins_)
{
    setup_signals();

    // Load history
    std::string home = env_.get("HOME");
    if (!home.empty()) hist_.load(home + "/.minibash_history");

    // Set shell-specific vars
    char cwd[4096];
    if (::getcwd(cwd, sizeof(cwd))) env_.set("PWD", cwd);
    env_.set("SHELL", "minibash");
    env_.set("MINIBASH_VERSION", "1.0.0");

    load_rc();
}

Shell::~Shell() {
    std::string home = env_.get("HOME");
    if (!home.empty()) hist_.save(home + "/.minibash_history");
}

void Shell::setup_signals() {
    struct sigaction sa{};

    // SIGINT (Ctrl-C): custom handler
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);

    // SIGCHLD: reap children
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // SIGQUIT, SIGTERM: ignore in shell
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

void Shell::load_rc() {
    std::string home = env_.get("HOME");
    if (home.empty()) return;
    std::string rc = home + "/.minibashrc";
    if (fs::exists(rc)) run_script(rc);
}

std::string Shell::prompt() const {
    // Build PS1-style prompt: user@host:cwd$
    std::string user = env_.get("USER");
    if (user.empty()) user = "user";

    char host[256];
    ::gethostname(host, sizeof(host));
    std::string hostname(host);
    auto dot = hostname.find('.');
    if (dot != std::string::npos) hostname = hostname.substr(0, dot);

    std::string pwd = env_.get("PWD");
    std::string home = env_.get("HOME");
    if (!home.empty() && pwd.rfind(home, 0) == 0)
        pwd = "~" + pwd.substr(home.size());

    // Colour: green if $?==0, red if not
    int status = env_.last_status();
    std::string colour = (status == 0) ? "\033[32m" : "\033[31m";
    std::string reset  = "\033[0m";
    std::string dollar = (::geteuid() == 0) ? "#" : "$";

    return "\033[1;34m" + user + "@" + hostname + reset + ":" +
           "\033[1;36m" + pwd + reset + " " + colour + dollar + reset + " ";
}

int Shell::run_line(const std::string& raw) {
    if (raw.empty() || raw[0] == '#') return 0;

    // History expansion
    std::string line = hist_.expand(raw);
    if (line.empty()) return 1;

    try {
        Lexer  lexer(line);
        auto   tokens = lexer.tokenise();
        Parser parser(std::move(tokens));
        CmdList list = parser.parse();
        if (list.items.empty()) return 0;
        return exec_.execute(list, *this);
    } catch (const std::exception& e) {
        std::cerr << "minibash: " << e.what() << "\n";
        return 1;
    }
}

std::string Shell::read_line(bool& eof) {
    eof = false;
    std::string ps1 = prompt();

    char* line = readline(ps1.c_str());
    if (!line) { eof = true; return ""; }

    std::string s(line);
    free(line);

    if (!s.empty()) {
        add_history(s.c_str());     // readline history
        hist_.add(s);               // our history
    }
    return s;
}

void Shell::run_interactive() {
    interactive_ = true;
    std::cout << "minibash 1.0.0  (type 'exit' to quit)\n";

    while (true) {
        // Reap completed background jobs and print notifications
        jobs_.reap();
        jobs_.cleanup();

        g_interrupted = 0;
        bool eof;
        std::string line = read_line(eof);

        if (eof) {
            std::cout << "exit\n"; break;
        }
        if (g_interrupted) continue;
        if (line.empty())  continue;

        int status = run_line(line);
        env_.set_last_status(status);
    }
}

int Shell::run_script(const std::string& path) {
    interactive_ = false;
    std::ifstream f(path);
    if (!f) {
        std::cerr << "minibash: " << path << ": cannot open\n";
        return 1;
    }
    std::string line;
    int status = 0;
    int lineno = 0;
    while (std::getline(f, line)) {
        lineno++;
        if (line.empty() || line[0] == '#') continue;
        try {
            status = run_line(line);
        } catch (const std::exception& e) {
            std::cerr << path << ":" << lineno << ": " << e.what() << "\n";
            return 1;
        }
    }
    return status;
}
