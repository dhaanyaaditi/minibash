#include "history.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

History::History(size_t max_size) : max_size_(max_size) {}

void History::add(const std::string& cmd) {
    if (cmd.empty()) return;
    // Don't duplicate consecutive identical commands
    if (!entries_.empty() && entries_.back() == cmd) return;
    if (entries_.size() >= max_size_) entries_.pop_front();
    entries_.push_back(cmd);
}

void History::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty()) {
            if (entries_.size() >= max_size_) entries_.pop_front();
            entries_.push_back(line);
        }
}

void History::save(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f) return;
    for (auto& e : entries_) f << e << "\n";
}

void History::print(int n) const {
    int start = 0;
    if (n > 0 && (int)entries_.size() > n)
        start = (int)entries_.size() - n;
    for (int i = start; i < (int)entries_.size(); i++)
        std::cout << "  " << std::setw(4) << (i + 1)
                  << "  " << entries_[i] << "\n";
}

const std::string& History::at(int i) const {
    // 1-based
    if (i < 1 || i > (int)entries_.size())
        throw std::out_of_range("history: no such entry: " + std::to_string(i));
    return entries_[i - 1];
}

const std::string& History::last() const {
    if (entries_.empty())
        throw std::runtime_error("history: no commands in history");
    return entries_.back();
}

std::string History::expand(const std::string& cmd) const {
    if (cmd.empty() || cmd[0] != '!') return cmd;

    // !! → last command
    if (cmd == "!!") {
        if (entries_.empty()) {
            std::cerr << "minibash: !!: event not found\n";
            return "";
        }
        std::string r = last();
        std::cout << r << "\n";
        return r;
    }

    // !N → Nth command
    if (cmd.size() > 1 && std::isdigit(cmd[1])) {
        try {
            int n = std::stoi(cmd.substr(1));
            std::string r = at(n);
            std::cout << r << "\n";
            return r;
        } catch (...) {
            std::cerr << "minibash: " << cmd << ": event not found\n";
            return "";
        }
    }

    // !string → most recent command starting with string
    std::string prefix = cmd.substr(1);
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->rfind(prefix, 0) == 0) {
            std::cout << *it << "\n";
            return *it;
        }
    }
    std::cerr << "minibash: " << cmd << ": event not found\n";
    return "";
}

std::optional<std::string> History::search(const std::string& prefix) const {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
        if (it->find(prefix) != std::string::npos)
            return *it;
    return std::nullopt;
}
