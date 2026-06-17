#pragma once
#include <string>
#include <vector>
#include <deque>
#include <optional>

/*  History: circular command history with persistence.
 *
 *  Features:
 *    - In-memory ring buffer (default 1000 entries)
 *    - Load/save from ~/.minibash_history
 *    - Reverse search (Ctrl-R style — implemented in readline wrapper)
 *    - !! (repeat last), !N (repeat Nth), !string (repeat matching)
 */
class History {
public:
    explicit History(size_t max_size = 1000);

    void add(const std::string& cmd);
    void load(const std::string& path);
    void save(const std::string& path) const;

    // Print history list (like bash 'history')
    void print(int n = -1) const; // n=-1 means all

    // Expand history references: !!, !N, !string
    // Returns expanded string or original if no match
    std::string expand(const std::string& cmd) const;

    size_t size()                    const { return entries_.size(); }
    const std::string& at(int i)     const; // 1-based
    const std::string& last()        const;

    // For readline Ctrl-R
    std::optional<std::string> search(const std::string& prefix) const;

private:
    std::deque<std::string> entries_;
    size_t                  max_size_;
};
