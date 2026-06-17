#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <stdexcept>

/*  Lexer: converts a raw input line into a flat list of Tokens.
 *
 *  Handles:
 *    - Single-quoted strings  → no expansion at all
 *    - Double-quoted strings  → variable expansion inside
 *    - Escape sequences       → \c outside quotes
 *    - Operators              → | ; & && || < > >> << ( )
 *    - Comments               → # outside quotes
 *    - Backslash-newline      → line continuation
 */
class Lexer {
public:
    explicit Lexer(const std::string& input) : input_(input), pos_(0) {}

    std::vector<Token> tokenise();

private:
    std::string input_;
    size_t      pos_;

    char peek(size_t offset = 0) const;
    char consume();
    bool at_end() const { return pos_ >= input_.size(); }

    Token read_word();
    Token read_operator();
    void  skip_whitespace();
    void  skip_comment();

    std::string read_single_quoted();
    std::string read_double_quoted();
    std::string read_heredoc_delim();
};
