#pragma once
#include "common.h"
#include "environment.h"
#include "jobs.h"
#include "history.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Shell; // forward declaration

/*  Builtins: commands that must run in the shell process (not forked).
 *
 *  Implemented:
 *    cd        chdir with OLDPWD tracking
 *    pwd       print working directory
 *    echo      print args (supports -n, -e)
 *    exit      exit with optional status
 *    export    mark variable for export / print exported vars
 *    unset     remove variable
 *    source/.  execute script in current shell
 *    history   print command history
 *    jobs      list background jobs
 *    fg        bring job to foreground
 *    bg        resume stopped job in background
 *    type      show how a name would be interpreted
 *    true/false return 0/1
 *    alias     define command aliases
 *    unalias   remove alias
 */
class Builtins {
public:
    Builtins(Environment& env, JobTable& jobs, History& hist)
        : env_(env), jobs_(jobs), hist_(hist) {}

    // Returns true if name is a builtin; sets exit_status
    bool is_builtin(const std::string& name) const;
    int  run(const std::vector<std::string>& argv, Shell& shell);

    // Alias management (public for expansion in executor)
    bool        has_alias(const std::string& name) const;
    std::string get_alias(const std::string& name) const;

private:
    Environment& env_;
    JobTable&    jobs_;
    History&     hist_;
    std::unordered_map<std::string, std::string> aliases_;

    int builtin_cd(const std::vector<std::string>& argv);
    int builtin_pwd(const std::vector<std::string>& argv);
    int builtin_echo(const std::vector<std::string>& argv);
    int builtin_export(const std::vector<std::string>& argv);
    int builtin_unset(const std::vector<std::string>& argv);
    int builtin_history(const std::vector<std::string>& argv);
    int builtin_jobs(const std::vector<std::string>& argv);
    int builtin_fg(const std::vector<std::string>& argv, Shell& shell);
    int builtin_bg(const std::vector<std::string>& argv);
    int builtin_type(const std::vector<std::string>& argv);
    int builtin_alias(const std::vector<std::string>& argv);
    int builtin_unalias(const std::vector<std::string>& argv);
    int builtin_source(const std::vector<std::string>& argv, Shell& shell);
};
