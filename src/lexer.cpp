#include "lexer.h"
#include <cctype>
#include <stdexcept>

char Lexer::peek(size_t offset) const {
    size_t i = pos_ + offset;
    return i < input_.size() ? input_[i] : '\0';
}

char Lexer::consume() {
    return input_[pos_++];
}

void Lexer::skip_whitespace() {
    while (!at_end() && (peek() == ' ' || peek() == '\t'))
        consume();
}

void Lexer::skip_comment() {
    while (!at_end() && peek() != '\n')
        consume();
}

std::string Lexer::read_single_quoted() {
    // consume opening '
    consume();
    std::string s;
    while (!at_end() && peek() != '\'')
        s += consume();
    if (at_end()) throw std::runtime_error("Unterminated single quote");
    consume(); // closing '
    return s;
}

std::string Lexer::read_double_quoted() {
    consume(); // opening "
    std::string s;
    while (!at_end() && peek() != '"') {
        if (peek() == '\\' && (peek(1) == '"' || peek(1) == '\\' ||
                                peek(1) == '$' || peek(1) == '`' ||
                                peek(1) == '\n')) {
            consume();            // backslash
            if (peek() == '\n') { consume(); continue; } // line continuation
            s += consume();
        } else {
            s += consume();
        }
    }
    if (at_end()) throw std::runtime_error("Unterminated double quote");
    consume(); // closing "
    return s;
}

Token Lexer::read_word() {
    std::string value;
    bool any = false;

    while (!at_end()) {
        char c = peek();

        // Single quote
        if (c == '\'') { value += read_single_quoted(); any = true; continue; }
        // Double quote
        if (c == '"')  { value += read_double_quoted(); any = true; continue; }
        // Backslash escape (outside quotes)
        if (c == '\\') {
            consume();
            if (!at_end()) {
                if (peek() == '\n') { consume(); continue; } // line continuation
                value += consume();
                any = true;
            }
            continue;
        }
        // Metacharacters end the word
        if (c == ' ' || c == '\t' || c == '\n' ||
            c == '|' || c == ';'  || c == '&'  ||
            c == '<' || c == '>'  || c == '('  || c == ')')
            break;
        // Comment — only at word boundary
        if (c == '#' && !any) break;

        value += consume();
        any = true;
    }

    return { TokenType::Word, value };
}

Token Lexer::read_operator() {
    char c = consume();
    char n = peek();

    if (c == '|') {
        if (n == '|') { consume(); return { TokenType::Or,  "||" }; }
        return { TokenType::Pipe, "|" };
    }
    if (c == '&') {
        if (n == '&') { consume(); return { TokenType::And, "&&" }; }
        return { TokenType::Amp, "&" };
    }
    if (c == ';') return { TokenType::Semi, ";" };
    if (c == '(') return { TokenType::LParen, "(" };
    if (c == ')') return { TokenType::RParen, ")" };
    if (c == '>') {
        if (n == '>') { consume(); return { TokenType::RedirOutApp, ">>" }; }
        return { TokenType::RedirOut, ">" };
    }
    if (c == '<') {
        if (n == '<') { consume(); return { TokenType::HereDoc, "<<" }; }
        return { TokenType::RedirIn, "<" };
    }
    if (c == '\n') return { TokenType::Newline, "\n" };
    return { TokenType::Word, std::string(1, c) };
}

std::vector<Token> Lexer::tokenise() {
    std::vector<Token> tokens;

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;

        char c = peek();
        if (c == '\n') { consume(); tokens.push_back({TokenType::Newline, "\n"}); continue; }
        if (c == '#')  { skip_comment(); continue; }

        if (c == '|' || c == '&' || c == ';' ||
            c == '<' || c == '>' || c == '(' || c == ')') {
            tokens.push_back(read_operator());
        } else {
            Token t = read_word();
            if (!t.value.empty()) tokens.push_back(t);
        }
    }

    tokens.push_back({ TokenType::EndOfInput, "" });
    return tokens;
}
