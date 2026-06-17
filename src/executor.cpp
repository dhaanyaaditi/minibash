#include "executor.h"
#include "shell.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sstream>
#include <stdexcept>

// ── Signal setup for child ────────────────────────────────────────────────────
void Executor::child_signals() {
    // Restore default handlers (shell ignores some)
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

// ── Word expansion ────────────────────────────────────────────────────────────
std::vector<std::string> Executor::expand_argv(const SimpleCmd& cmd) {
    std::vector<std::string> argv;
    for (auto& w : cmd.words) {
        std::string expanded = env_.expand(w.raw);
        // Glob expand only for unquoted words
        if (!w.quoted) {
            auto globs = env_.glob_expand(expanded);
            for (auto& g : globs) argv.push_back(g);
        } else {
            argv.push_back(expanded);
        }
    }
    return argv;
}

// ── Redirections ──────────────────────────────────────────────────────────────
bool Executor::apply_redirects(const std::vector<Redirect>& redirs) {
    for (auto& r : redirs) {
        int fd = -1;
        switch (r.type) {
        case RedirType::RedirOut:
            fd = ::open(r.target.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(r.target.c_str()); return false; }
            ::dup2(fd, STDOUT_FILENO);
            ::close(fd);
            break;
        case RedirType::RedirOutAppend:
            fd = ::open(r.target.c_str(),
                        O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror(r.target.c_str()); return false; }
            ::dup2(fd, STDOUT_FILENO);
            ::close(fd);
            break;
        case RedirType::RedirIn:
            fd = ::open(r.target.c_str(), O_RDONLY);
            if (fd < 0) { perror(r.target.c_str()); return false; }
            ::dup2(fd, STDIN_FILENO);
            ::close(fd);
            break;
        case RedirType::HereDoc: {
            // Here-doc: r.target holds the content (pre-read by shell)
            int pipefd[2];
            ::pipe(pipefd);
            ::write(pipefd[1], r.target.c_str(), r.target.size());
            ::close(pipefd[1]);
            ::dup2(pipefd[0], STDIN_FILENO);
            ::close(pipefd[0]);
            break;
        }
        }
    }
    return true;
}

// ── Execute simple command ────────────────────────────────────────────────────
int Executor::exec_simple(const SimpleCmd& cmd, int in_fd, int out_fd,
                          Shell& shell) {
    auto argv = expand_argv(cmd);
    if (argv.empty()) return 0;

    // Check aliases
    if (builtins_.has_alias(argv[0])) {
        std::string expanded = builtins_.get_alias(argv[0]);
        // Prepend alias expansion
        std::vector<std::string> new_argv;
        std::istringstream ss(expanded);
        std::string tok;
        while (ss >> tok) new_argv.push_back(tok);
        for (size_t i = 1; i < argv.size(); i++) new_argv.push_back(argv[i]);
        argv = new_argv;
    }

    // Builtin — run directly (no fork) when not in pipeline
    bool in_pipeline = (in_fd != STDIN_FILENO || out_fd != STDOUT_FILENO);
    if (!in_pipeline && builtins_.is_builtin(argv[0])) {
        return builtins_.run(argv, shell);
    }

    // Fork
    pid_t pid = ::fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        // ── Child process ─────────────────────────────────────────────
        child_signals();

        // Put child in its own process group (for job control)
        ::setpgid(0, 0);

        // Hook up pipe fds
        if (in_fd != STDIN_FILENO) {
            ::dup2(in_fd, STDIN_FILENO);
            ::close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            ::dup2(out_fd, STDOUT_FILENO);
            ::close(out_fd);
        }

        // Apply redirections
        if (!apply_redirects(cmd.redirs)) ::_exit(1);

        // Try builtin (needed when in pipeline)
        if (builtins_.is_builtin(argv[0])) {
            int ret = builtins_.run(argv, shell);
            ::_exit(ret);
        }

        // External command
        auto path = env_.find_in_path(argv[0]);
        if (!path) {
            std::cerr << "minibash: " << argv[0] << ": command not found\n";
            ::_exit(EXIT_CMD_NOT_FOUND);
        }

        // Build argv and envp for execve
        std::vector<char*> cargv;
        for (auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);

        auto envp = env_.envp();
        ::execve(path->c_str(), cargv.data(), envp.data());
        perror(argv[0].c_str());
        ::_exit(1);
    }

    // ── Parent process ────────────────────────────────────────────────────
    ::setpgid(pid, pid); // ensure child is in its own group

    // Close pipe ends we no longer need
    if (in_fd  != STDIN_FILENO)  ::close(in_fd);
    if (out_fd != STDOUT_FILENO) ::close(out_fd);

    if (cmd.background) {
        jobs_.add(pid, argv[0]);
        env_.set_last_bg_pid(pid);
        return 0;
    }

    // Foreground: give terminal to child, wait
    ::tcsetpgrp(terminal_fd_, pid);
    int status = jobs_.wait_for(pid);
    ::tcsetpgrp(terminal_fd_, ::getpgrp()); // reclaim terminal

    if (WIFSIGNALED(status) && WTERMSIG(status) != SIGPIPE)
        std::cout << "\n";

    return WIFEXITED(status) ? WEXITSTATUS(status)
         : WIFSIGNALED(status) ? EXIT_SIGNAL_BASE + WTERMSIG(status)
         : status;
}

// ── Execute pipeline ──────────────────────────────────────────────────────────
int Executor::exec_pipeline(const Pipeline& pl, Shell& shell) {
    size_t n = pl.cmds.size();
    if (n == 0) return 0;
    if (n == 1) {
        int ret = exec_simple(pl.cmds[0], STDIN_FILENO, STDOUT_FILENO, shell);
        return pl.negate ? !ret : ret;
    }

    // Build N-1 pipes
    std::vector<std::array<int,2>> pipes(n - 1);
    for (auto& p : pipes)
        if (::pipe(p.data()) < 0) { perror("pipe"); return 1; }

    std::vector<pid_t> pids(n);

    for (size_t i = 0; i < n; i++) {
        int in_fd  = (i == 0)   ? STDIN_FILENO  : pipes[i-1][0];
        int out_fd = (i == n-1) ? STDOUT_FILENO : pipes[i][1];

        pid_t pid = ::fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            // Child
            child_signals();
            ::setpgid(0, 0);

            if (in_fd  != STDIN_FILENO)  { ::dup2(in_fd,  STDIN_FILENO);  ::close(in_fd); }
            if (out_fd != STDOUT_FILENO) { ::dup2(out_fd, STDOUT_FILENO); ::close(out_fd); }

            // Close all pipe fds not needed by this child
            for (size_t j = 0; j < pipes.size(); j++) {
                if ((int)j != (int)i-1) ::close(pipes[j][0]);
                if (j != i)             ::close(pipes[j][1]);
            }

            if (!apply_redirects(pl.cmds[i].redirs)) ::_exit(1);

            auto argv = expand_argv(pl.cmds[i]);
            if (builtins_.is_builtin(argv[0])) {
                ::_exit(builtins_.run(argv, shell));
            }

            auto path = env_.find_in_path(argv[0]);
            if (!path) {
                std::cerr << "minibash: " << argv[0] << ": command not found\n";
                ::_exit(EXIT_CMD_NOT_FOUND);
            }
            std::vector<char*> cargv;
            for (auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
            cargv.push_back(nullptr);
            auto envp = env_.envp();
            ::execve(path->c_str(), cargv.data(), envp.data());
            perror(argv[0].c_str());
            ::_exit(1);
        }
        pids[i] = pid;
        ::setpgid(pid, pids[0]);
    }

    // Parent: close all pipe fds
    for (auto& p : pipes) { ::close(p[0]); ::close(p[1]); }

    // Check if last cmd is background
    bool bg = pl.cmds.back().background;
    if (bg) {
        jobs_.add(pids[0], cmd_string(pl));
        return 0;
    }

    // Give terminal to pipeline group, wait for all
    ::tcsetpgrp(terminal_fd_, pids[0]);
    int last_status = 0;
    for (pid_t pid : pids) {
        int s;
        ::waitpid(pid, &s, WUNTRACED);
        if (WIFEXITED(s))   last_status = WEXITSTATUS(s);
        if (WIFSIGNALED(s)) last_status = EXIT_SIGNAL_BASE + WTERMSIG(s);
    }
    ::tcsetpgrp(terminal_fd_, ::getpgrp());

    return pl.negate ? !last_status : last_status;
}

// ── Execute command list ──────────────────────────────────────────────────────
int Executor::execute(const CmdList& list, Shell& shell) {
    int status = 0;
    for (size_t i = 0; i < list.items.size(); i++) {
        auto& item = list.items[i];

        // Short-circuit &&: if previous failed, skip
        if (i > 0 && list.items[i-1].op == ListOp::And && status != 0)
            continue;
        // Short-circuit ||: if previous succeeded, skip
        if (i > 0 && list.items[i-1].op == ListOp::Or && status == 0)
            continue;

        status = exec_pipeline(item.pill, shell);
        env_.set_last_status(status);
    }
    return status;
}

std::string Executor::cmd_string(const Pipeline& pl) {
    std::string s;
    for (auto& cmd : pl.cmds) {
        if (!s.empty()) s += " | ";
        for (auto& w : cmd.words) s += w.raw + " ";
    }
    return s;
}
