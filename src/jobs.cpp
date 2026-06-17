#include "jobs.h"
#include "common.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>

JobTable::JobTable() {}

int JobTable::next_jid() const {
    int max = 0;
    for (auto& j : jobs_) max = std::max(max, j.jid);
    return max + 1;
}

int JobTable::add(pid_t pgid, const std::string& cmd) {
    std::lock_guard<std::mutex> lk(mtx_);
    Job j;
    j.jid   = next_jid();
    j.pgid  = pgid;
    j.cmd   = cmd;
    j.state = JobState::Running;
    jobs_.push_back(j);
    std::cout << "[" << j.jid << "] " << pgid << "\n";
    return j.jid;
}

Job* JobTable::find(int jid) {
    for (auto& j : jobs_) if (j.jid == jid) return &j;
    return nullptr;
}

Job* JobTable::find_by_pgid(pid_t pgid) {
    for (auto& j : jobs_) if (j.pgid == pgid) return &j;
    return nullptr;
}

std::string JobTable::state_str(JobState s) {
    switch (s) {
        case JobState::Running: return "Running";
        case JobState::Stopped: return "Stopped";
        case JobState::Done:    return "Done";
    }
    return "?";
}

void JobTable::reap() {
    int status;
    pid_t pid;
    // WNOHANG + WUNTRACED: non-blocking, also catch stops
    while ((pid = ::waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        std::lock_guard<std::mutex> lk(mtx_);
        Job* j = find_by_pgid(pid);
        if (!j) {
            // Try to find by checking each job's pgid
            for (auto& job : jobs_) {
                if (job.pgid == pid || ::kill(-job.pgid, 0) == -1) {
                    j = &job; break;
                }
            }
        }
        if (!j) continue;

        if (WIFSTOPPED(status)) {
            j->state = JobState::Stopped;
            std::cout << "\n[" << j->jid << "]+ Stopped\t" << j->cmd << "\n";
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            j->state = JobState::Done;
            j->exit_status = WIFEXITED(status) ? WEXITSTATUS(status)
                                                : EXIT_SIGNAL_BASE + WTERMSIG(status);
        }
    }
}

int JobTable::wait_for(pid_t pgid) {
    int status = 0;
    pid_t pid;
    do {
        pid = ::waitpid(-pgid, &status, WUNTRACED);
    } while (pid > 0 && !WIFEXITED(status) && !WIFSIGNALED(status) &&
             !WIFSTOPPED(status));

    if (WIFSTOPPED(status)) {
        std::lock_guard<std::mutex> lk(mtx_);
        Job* j = find_by_pgid(pgid);
        if (j) {
            j->state = JobState::Stopped;
            std::cout << "\n[" << j->jid << "]+ Stopped\t" << j->cmd << "\n";
        }
        return 128 + WSTOPSIG(status);
    }
    if (WIFSIGNALED(status)) return EXIT_SIGNAL_BASE + WTERMSIG(status);
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    return 0;
}

bool JobTable::fg(int jid, int terminal_fd) {
    std::lock_guard<std::mutex> lk(mtx_);
    Job* j = find(jid);
    if (!j) { std::cerr << "minibash: fg: " << jid << ": no such job\n"; return false; }

    std::cout << j->cmd << "\n";
    // Give terminal to job's process group
    ::tcsetpgrp(terminal_fd, j->pgid);
    // Send SIGCONT to wake it up if stopped
    ::kill(-j->pgid, SIGCONT);
    j->state = JobState::Running;
    return true;
}

bool JobTable::bg(int jid) {
    std::lock_guard<std::mutex> lk(mtx_);
    Job* j = find(jid);
    if (!j) { std::cerr << "minibash: bg: " << jid << ": no such job\n"; return false; }
    if (j->state == JobState::Running) {
        std::cerr << "minibash: bg: job " << jid << " already in background\n";
        return false;
    }
    ::kill(-j->pgid, SIGCONT);
    j->state = JobState::Running;
    std::cout << "[" << j->jid << "]+ " << j->cmd << " &\n";
    return true;
}

void JobTable::list() const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& j : jobs_) {
        if (j.state == JobState::Done) continue;
        std::cout << "[" << j.jid << "]  "
                  << std::left << std::setw(10) << state_str(j.state)
                  << j.cmd << "\n";
    }
}

void JobTable::cleanup() {
    std::lock_guard<std::mutex> lk(mtx_);
    // Notify done jobs, then remove
    for (auto& j : jobs_) {
        if (j.state == JobState::Done)
            std::cout << "[" << j.jid << "]  Done\t\t" << j.cmd << "\n";
    }
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
        [](const Job& j){ return j.state == JobState::Done; }), jobs_.end());
}
