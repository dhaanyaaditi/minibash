#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/*  Environment: manages shell variables and the process environment.
 *
 *  Handles:
 *    - Variable set/get/unset
 *    - export (mark vars for child processes)
 *    - $VAR and ${VAR} expansion
 *    - $? (last exit status)
 *    - $$ (shell PID)
 *    - $! (last background PID)
 *    - ~ (HOME) expansion
 *    - Glob expansion (* ? [...])
 */
class Environment {
public:
    Environment();

    // Variable access
    void        set(const std::string& name, const std::string& val,
                    bool exported = false);
    std::string get(const std::string& name) const;
    bool        has(const std::string& name) const;
    void        unset(const std::string& name);
    void        export_var(const std::string& name);
    bool        is_exported(const std::string& name) const;

    // Special variables
    void set_last_status(int s)      { last_status_ = s; }
    int  last_status()         const { return last_status_; }
    void set_last_bg_pid(pid_t p)    { last_bg_pid_ = p; }
    pid_t last_bg_pid()        const { return last_bg_pid_; }

    // Expand $VAR, ${VAR}, $?, $$, $!, ~ in a word
    std::string expand(const std::string& word) const;

    // Expand globs in a word → list of matching filenames (or original)
    std::vector<std::string> glob_expand(const std::string& word) const;

    // Build envp[] for execve
    std::vector<char*> envp() const;
    void free_envp(std::vector<char*>& ep) const;

    // PATH lookup: find full path of a command
    std::optional<std::string> find_in_path(const std::string& cmd) const;

    // Print all variables (for 'export' builtin with no args)
    void print_all(bool exported_only = false) const;

    // Load from current process environment
    void load_from_environ();

private:
    struct VarEntry {
        std::string value;
        bool        exported{false};
    };

    std::unordered_map<std::string, VarEntry> vars_;
    int   last_status_{0};
    pid_t last_bg_pid_{-1};

    std::string expand_var(const std::string& name) const;
};
