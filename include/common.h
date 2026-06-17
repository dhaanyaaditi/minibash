#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>

// ── Exit / signal codes ───────────────────────────────────────────────────────
constexpr int EXIT_SUCCESS_CODE = 0;
constexpr int EXIT_FAILURE_CODE = 1;
constexpr int EXIT_CMD_NOT_FOUND = 127;
constexpr int EXIT_SIGNAL_BASE   = 128;  // 128+N for signal N

// ── Redirection types ─────────────────────────────────────────────────────────
enum class RedirType {
    RedirOut,       // >
    RedirOutAppend, // >>
    RedirIn,        // <
    HereDoc,        // <<
};

struct Redirect {
    RedirType   type;
    std::string target;  // filename or here-doc delimiter
    int         fd{-1};  // explicit fd (e.g. 2>file uses fd=2), -1 = default
};

// ── Token ─────────────────────────────────────────────────────────────────────
enum class TokenType {
    Word,       // regular word / quoted string
    Pipe,       // |
    Semi,       // ;
    Amp,        // &   (background)
    And,        // &&
    Or,         // ||
    LParen,     // (
    RParen,     // )
    RedirOut,   // >
    RedirOutApp,// >>
    RedirIn,    // <
    HereDoc,    // <<
    Newline,
    EndOfInput,
};

struct Token {
    TokenType   type;
    std::string value;
};

// ── Command word (after expansion) ───────────────────────────────────────────
struct Word {
    std::string raw;
    bool        quoted{false};   // was in single/double quotes
};

// ── Simple command ────────────────────────────────────────────────────────────
struct SimpleCmd {
    std::vector<Word>     words;      // argv[0] is command name
    std::vector<Redirect> redirs;
    bool                  background{false};
};

// ── Pipeline ──────────────────────────────────────────────────────────────────
struct Pipeline {
    std::vector<SimpleCmd> cmds;     // connected by pipes
    bool                   negate{false}; // leading !
};

// ── Command list (pipelines joined by ; && ||) ────────────────────────────────
enum class ListOp { Semi, And, Or };

struct CmdList {
    struct Item {
        Pipeline pill;
        std::optional<ListOp> op; // operator after this pipeline
    };
    std::vector<Item> items;
};
