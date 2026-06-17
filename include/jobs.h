#pragma once
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

enum class JobState { Running, Stopped, Done };

struct Job {
    int         jid;       // job ID (1-based, shown in [1])
    pid_t       pgid;      // process group ID
    std::string cmd;       // command string for display
    JobState    state{JobState::Running};
    int         exit_status{0};
};

/*  JobTable: tracks background and suspended jobs.
 *
 *  On SIGCHLD we reap children and update state.
 *  fg  → bring job to foreground (SIGCONT + tcsetpgrp)
 *  bg  → resume stopped job in background (SIGCONT)
 *  jobs→ list all jobs
 */
class JobTable {
public:
    JobTable();

    // Add a new background job; returns job ID
    int  add(pid_t pgid, const std::string& cmd);

    // Called from SIGCHLD handler — reap any finished children
    void reap();

    // Wait for a specific pgid (foreground process)
    int  wait_for(pid_t pgid);

    // Bring job to foreground
    bool fg(int jid, int terminal_fd);

    // Resume stopped job in background
    bool bg(int jid);

    // Print job list (like bash 'jobs')
    void list() const;

    // Remove completed jobs from table
    void cleanup();

    int  next_jid() const;

private:
    std::vector<Job>  jobs_;
    mutable std::mutex mtx_;

    Job* find(int jid);
    Job* find_by_pgid(pid_t pgid);

    static std::string state_str(JobState s);
};
