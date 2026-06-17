#include "builtins.h"
#include "shell.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <sstream>

static bool is_valid_varname(const std::string& s) {
    if (s.empty() || std::isdigit(s[0])) return false;
    return std::all_of(s.begin(), s.end(),
        [](char c){ return std::isalnum(c) || c == '_'; });
}

bool Builtins::is_builtin(const std::string& name) const {
    static const std::vector<std::string> names = {
        "cd","pwd","echo","exit","export","unset","source",".",
        "history","jobs","fg","bg","type","true","false",
        "alias","unalias",":", "read"
    };
    return std::find(names.begin(), names.end(), name) != names.end();
}

bool Builtins::has_alias(const std::string& n) const {
    return aliases_.count(n) > 0;
}
std::string Builtins::get_alias(const std::string& n) const {
    auto it = aliases_.find(n);
    return it != aliases_.end() ? it->second : "";
}

int Builtins::run(const std::vector<std::string>& argv, Shell& shell) {
    if (argv.empty()) return 0;
    const std::string& cmd = argv[0];

    if (cmd == "cd")      return builtin_cd(argv);
    if (cmd == "pwd")     return builtin_pwd(argv);
    if (cmd == "echo")    return builtin_echo(argv);
    if (cmd == "export")  return builtin_export(argv);
    if (cmd == "unset")   return builtin_unset(argv);
    if (cmd == "history") return builtin_history(argv);
    if (cmd == "jobs")    return builtin_jobs(argv);
    if (cmd == "fg")      return builtin_fg(argv, shell);
    if (cmd == "bg")      return builtin_bg(argv);
    if (cmd == "type")    return builtin_type(argv);
    if (cmd == "alias")   return builtin_alias(argv);
    if (cmd == "unalias") return builtin_unalias(argv);
    if (cmd == "source" || cmd == ".") return builtin_source(argv, shell);
    if (cmd == "true"  || cmd == ":") return 0;
    if (cmd == "false")   return 1;
    if (cmd == "exit") {
        int code = (argv.size() > 1) ? std::stoi(argv[1]) : env_.last_status();
        hist_.save(env_.get("HOME") + "/.minibash_history");
        std::cout << "exit\n";
        ::exit(code);
    }
    if (cmd == "read") {
        if (argv.size() < 2) return 1;
        std::string line;
        std::getline(std::cin, line);
        env_.set(argv[1], line);
        return 0;
    }
    return 1;
}

int Builtins::builtin_cd(const std::vector<std::string>& argv) {
    std::string target;
    if (argv.size() < 2 || argv[1] == "~") {
        target = env_.get("HOME");
        if (target.empty()) { std::cerr << "cd: HOME not set\n"; return 1; }
    } else if (argv[1] == "-") {
        target = env_.get("OLDPWD");
        if (target.empty()) { std::cerr << "cd: OLDPWD not set\n"; return 1; }
        std::cout << target << "\n";
    } else {
        target = argv[1];
    }

    char cwd[4096];
    ::getcwd(cwd, sizeof(cwd));
    env_.set("OLDPWD", cwd);

    if (::chdir(target.c_str()) < 0) {
        std::cerr << "cd: " << target << ": " << strerror(errno) << "\n";
        return 1;
    }
    ::getcwd(cwd, sizeof(cwd));
    env_.set("PWD", cwd);
    return 0;
}

int Builtins::builtin_pwd(const std::vector<std::string>&) {
    char cwd[4096];
    if (::getcwd(cwd, sizeof(cwd))) std::cout << cwd << "\n";
    else { perror("pwd"); return 1; }
    return 0;
}

int Builtins::builtin_echo(const std::vector<std::string>& argv) {
    bool newline = true;
    bool escape  = false;
    size_t start = 1;

    // Parse flags
    while (start < argv.size() && argv[start][0] == '-') {
        bool valid = true;
        for (char c : argv[start].substr(1)) {
            if (c == 'n') newline = false;
            else if (c == 'e') escape = true;
            else if (c == 'E') escape = false;
            else { valid = false; break; }
        }
        if (!valid) break;
        start++;
    }

    for (size_t i = start; i < argv.size(); i++) {
        if (i > start) std::cout << ' ';
        if (escape) {
            for (size_t j = 0; j < argv[i].size(); j++) {
                if (argv[i][j] == '\\' && j+1 < argv[i].size()) {
                    char nc = argv[i][++j];
                    switch(nc) {
                        case 'n': std::cout << '\n'; break;
                        case 't': std::cout << '\t'; break;
                        case 'r': std::cout << '\r'; break;
                        case '\\':std::cout << '\\'; break;
                        default:  std::cout << '\\' << nc;
                    }
                } else {
                    std::cout << argv[i][j];
                }
            }
        } else {
            std::cout << argv[i];
        }
    }
    if (newline) std::cout << '\n';
    return 0;
}

int Builtins::builtin_export(const std::vector<std::string>& argv) {
    if (argv.size() == 1) { env_.print_all(true); return 0; }
    for (size_t i = 1; i < argv.size(); i++) {
        auto eq = argv[i].find('=');
        if (eq != std::string::npos) {
            std::string name = argv[i].substr(0, eq);
            std::string val  = argv[i].substr(eq + 1);
            if (!is_valid_varname(name)) {
                std::cerr << "export: `" << name << "': not a valid identifier\n";
                return 1;
            }
            env_.set(name, val, true);
        } else {
            env_.export_var(argv[i]);
        }
    }
    return 0;
}

int Builtins::builtin_unset(const std::vector<std::string>& argv) {
    for (size_t i = 1; i < argv.size(); i++) env_.unset(argv[i]);
    return 0;
}

int Builtins::builtin_history(const std::vector<std::string>& argv) {
    int n = -1;
    if (argv.size() > 1) {
        try { n = std::stoi(argv[1]); }
        catch (...) { std::cerr << "history: invalid number\n"; return 1; }
    }
    hist_.print(n);
    return 0;
}

int Builtins::builtin_jobs(const std::vector<std::string>&) {
    jobs_.list();
    return 0;
}

int Builtins::builtin_fg(const std::vector<std::string>& argv, Shell& shell) {
    int jid = 1;
    if (argv.size() > 1) {
        std::string a = argv[1];
        if (a[0] == '%') a = a.substr(1);
        try { jid = std::stoi(a); } catch (...) { return 1; }
    }
    jobs_.fg(jid, STDIN_FILENO);
    // After fg returns (job done/stopped), wait for status update
    int status = jobs_.wait_for(jid);
    ::tcsetpgrp(STDIN_FILENO, ::getpgrp());
    return status;
}

int Builtins::builtin_bg(const std::vector<std::string>& argv) {
    int jid = 1;
    if (argv.size() > 1) {
        std::string a = argv[1];
        if (a[0] == '%') a = a.substr(1);
        try { jid = std::stoi(a); } catch (...) { return 1; }
    }
    return jobs_.bg(jid) ? 0 : 1;
}

int Builtins::builtin_type(const std::vector<std::string>& argv) {
    if (argv.size() < 2) { std::cerr << "type: missing argument\n"; return 1; }
    int ret = 0;
    for (size_t i = 1; i < argv.size(); i++) {
        if (is_builtin(argv[i])) {
            std::cout << argv[i] << " is a shell builtin\n";
        } else if (has_alias(argv[i])) {
            std::cout << argv[i] << " is aliased to `" << get_alias(argv[i]) << "'\n";
        } else {
            auto path = env_.find_in_path(argv[i]);
            if (path) std::cout << argv[i] << " is " << *path << "\n";
            else { std::cerr << argv[i] << ": not found\n"; ret = 1; }
        }
    }
    return ret;
}

int Builtins::builtin_alias(const std::vector<std::string>& argv) {
    if (argv.size() == 1) {
        for (auto& [k,v] : aliases_)
            std::cout << "alias " << k << "='" << v << "'\n";
        return 0;
    }
    for (size_t i = 1; i < argv.size(); i++) {
        auto eq = argv[i].find('=');
        if (eq == std::string::npos) {
            auto it = aliases_.find(argv[i]);
            if (it != aliases_.end())
                std::cout << "alias " << argv[i] << "='" << it->second << "'\n";
            else { std::cerr << "alias: " << argv[i] << " not found\n"; }
        } else {
            std::string name = argv[i].substr(0, eq);
            std::string val  = argv[i].substr(eq + 1);
            // Strip surrounding quotes from value
            if (val.size() >= 2 &&
                ((val.front()=='\'' && val.back()=='\'') ||
                 (val.front()=='"'  && val.back()=='"')))
                val = val.substr(1, val.size()-2);
            aliases_[name] = val;
        }
    }
    return 0;
}

int Builtins::builtin_unalias(const std::vector<std::string>& argv) {
    if (argv.size() < 2) { std::cerr << "unalias: missing argument\n"; return 1; }
    for (size_t i = 1; i < argv.size(); i++) aliases_.erase(argv[i]);
    return 0;
}

int Builtins::builtin_source(const std::vector<std::string>& argv, Shell& shell) {
    if (argv.size() < 2) { std::cerr << "source: missing filename\n"; return 1; }
    std::ifstream f(argv[1]);
    if (!f) { std::cerr << "source: " << argv[1] << ": no such file\n"; return 1; }
    std::string line;
    int status = 0;
    while (std::getline(f, line))
        status = shell.run_line(line);
    return status;
}
