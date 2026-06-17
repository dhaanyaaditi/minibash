#include "environment.h"
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <glob.h>
#include <pwd.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

Environment::Environment() {
    load_from_environ();
}

void Environment::load_from_environ() {
    extern char** environ;
    for (char** e = environ; e && *e; e++) {
        std::string entry(*e);
        auto eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string name  = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);
        vars_[name] = { value, true };
    }
}

void Environment::set(const std::string& name, const std::string& val,
                      bool exported) {
    auto& e = vars_[name];
    e.value    = val;
    e.exported = e.exported || exported;
}

std::string Environment::get(const std::string& name) const {
    auto it = vars_.find(name);
    return it != vars_.end() ? it->second.value : "";
}

bool Environment::has(const std::string& name) const {
    return vars_.count(name) > 0;
}

void Environment::unset(const std::string& name) {
    vars_.erase(name);
}

void Environment::export_var(const std::string& name) {
    vars_[name].exported = true;
}

bool Environment::is_exported(const std::string& name) const {
    auto it = vars_.find(name);
    return it != vars_.end() && it->second.exported;
}

std::string Environment::expand_var(const std::string& name) const {
    if (name == "?")  return std::to_string(last_status_);
    if (name == "$")  return std::to_string(::getpid());
    if (name == "!")  return last_bg_pid_ < 0 ? "" : std::to_string(last_bg_pid_);
    if (name == "0")  return "minibash";
    return get(name);
}

std::string Environment::expand(const std::string& word) const {
    std::string result;
    size_t i = 0;

    while (i < word.size()) {
        // Tilde expansion at start
        if (i == 0 && word[i] == '~') {
            std::string home = get("HOME");
            if (home.empty()) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home = pw->pw_dir;
            }
            result += home;
            i++;
            continue;
        }

        if (word[i] != '$') { result += word[i++]; continue; }
        i++; // skip $

        if (i >= word.size()) { result += '$'; break; }

        // ${VAR} or $VAR
        if (word[i] == '{') {
            i++; // skip {
            size_t start = i;
            while (i < word.size() && word[i] != '}') i++;
            std::string name = word.substr(start, i - start);
            if (i < word.size()) i++; // skip }
            result += expand_var(name);
        } else if (word[i] == '?' || word[i] == '$' || word[i] == '!') {
            result += expand_var(std::string(1, word[i]));
            i++;
        } else if (std::isalpha(word[i]) || word[i] == '_') {
            size_t start = i;
            while (i < word.size() && (std::isalnum(word[i]) || word[i] == '_'))
                i++;
            result += expand_var(word.substr(start, i - start));
        } else {
            result += '$';
        }
    }
    return result;
}

std::vector<std::string> Environment::glob_expand(const std::string& word) const {
    // Only glob if word contains * ? [
    if (word.find_first_of("*?[") == std::string::npos)
        return { word };

    glob_t g{};
    int ret = ::glob(word.c_str(), GLOB_TILDE | GLOB_NOCHECK, nullptr, &g);
    std::vector<std::string> results;

    if (ret == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++)
            results.emplace_back(g.gl_pathv[i]);
    } else {
        results.push_back(word); // no match → return as-is (bash behaviour)
    }
    globfree(&g);
    return results;
}

std::optional<std::string> Environment::find_in_path(const std::string& cmd) const {
    // Absolute or relative path
    if (cmd.find('/') != std::string::npos) {
        if (::access(cmd.c_str(), X_OK) == 0) return cmd;
        return std::nullopt;
    }

    std::string path_env = get("PATH");
    if (path_env.empty()) path_env = "/usr/local/bin:/usr/bin:/bin";

    std::istringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::string full = dir + "/" + cmd;
        if (::access(full.c_str(), X_OK) == 0) return full;
    }
    return std::nullopt;
}

std::vector<char*> Environment::envp() const {
    std::vector<char*> ep;
    for (auto& [name, entry] : vars_) {
        if (!entry.exported) continue;
        std::string s = name + "=" + entry.value;
        char* buf = new char[s.size() + 1];
        std::memcpy(buf, s.c_str(), s.size() + 1);
        ep.push_back(buf);
    }
    ep.push_back(nullptr);
    return ep;
}

void Environment::free_envp(std::vector<char*>& ep) const {
    for (char* p : ep) delete[] p;
    ep.clear();
}

void Environment::print_all(bool exported_only) const {
    std::vector<std::string> names;
    for (auto& [n, _] : vars_) names.push_back(n);
    std::sort(names.begin(), names.end());

    for (auto& n : names) {
        auto& e = vars_.at(n);
        if (exported_only && !e.exported) continue;
        if (exported_only)
            std::cout << "declare -x " << n << "=\"" << e.value << "\"\n";
        else
            std::cout << n << "=" << e.value << "\n";
    }
}
