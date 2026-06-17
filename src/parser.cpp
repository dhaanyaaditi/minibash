#include "parser.h"
#include <stdexcept>

const Token& Parser::peek(size_t offset) const {
    size_t i = pos_ + offset;
    if (i >= tokens_.size()) return tokens_.back(); // EndOfInput
    return tokens_[i];
}

const Token& Parser::consume() {
    return tokens_[pos_++];
}

bool Parser::at_end() const {
    return pos_ >= tokens_.size() ||
           tokens_[pos_].type == TokenType::EndOfInput ||
           tokens_[pos_].type == TokenType::Newline;
}

Word Parser::parse_word() {
    const Token& t = consume();
    return { t.value, false };
}

Redirect Parser::parse_redirect() {
    const Token& op = consume();
    if (at_end() || peek().type != TokenType::Word)
        throw std::runtime_error("Expected filename after redirection operator");

    Redirect r;
    r.target = consume().value;

    switch (op.type) {
        case TokenType::RedirOut:    r.type = RedirType::RedirOut; break;
        case TokenType::RedirOutApp: r.type = RedirType::RedirOutAppend; break;
        case TokenType::RedirIn:     r.type = RedirType::RedirIn; break;
        case TokenType::HereDoc:     r.type = RedirType::HereDoc; break;
        default: throw std::runtime_error("Unknown redirection operator");
    }
    return r;
}

SimpleCmd Parser::parse_simple_cmd() {
    SimpleCmd cmd;

    while (!at_end()) {
        TokenType t = peek().type;

        if (t == TokenType::Word) {
            cmd.words.push_back(parse_word());
        }
        else if (t == TokenType::RedirOut || t == TokenType::RedirOutApp ||
                 t == TokenType::RedirIn  || t == TokenType::HereDoc) {
            cmd.redirs.push_back(parse_redirect());
        }
        else {
            break;
        }
    }
    return cmd;
}

Pipeline Parser::parse_pipeline() {
    Pipeline pl;

    // Leading !
    if (!at_end() && peek().type == TokenType::Word &&
        peek().value == "!") {
        consume();
        pl.negate = true;
    }

    pl.cmds.push_back(parse_simple_cmd());

    while (!at_end() && peek().type == TokenType::Pipe) {
        consume(); // consume |
        pl.cmds.push_back(parse_simple_cmd());
    }
    return pl;
}

CmdList Parser::parse() {
    CmdList list;

    // Skip leading newlines/semis
    while (!at_end() &&
           (peek().type == TokenType::Newline ||
            peek().type == TokenType::Semi))
        consume();

    if (at_end()) return list;

    CmdList::Item item;
    item.pill = parse_pipeline();

    while (!at_end()) {
        TokenType t = peek().type;

        if (t == TokenType::Semi || t == TokenType::Newline) {
            consume();
            item.op = ListOp::Semi;
            list.items.push_back(item);
            if (at_end()) break;
            item = {};
            item.pill = parse_pipeline();
        }
        else if (t == TokenType::And) {
            consume();
            // Check for background (&) vs && 
            item.op = ListOp::Semi; // & treated as semi (background handled elsewhere)
            item.pill.cmds.back().background = true;
            list.items.push_back(item);
            if (at_end()) break;
            item = {};
            item.pill = parse_pipeline();
        }
        else if (t == TokenType::Amp) {
            consume();
            item.pill.cmds.back().background = true;
            item.op = ListOp::Semi;
            list.items.push_back(item);
            if (at_end()) break;
            item = {};
            item.pill = parse_pipeline();
        }
        else if (t == TokenType::Or) {
            consume();
            item.op = ListOp::Or;
            list.items.push_back(item);
            item = {};
            item.pill = parse_pipeline();
        }
        else {
            // Check for && 
            if (t == TokenType::And) {
                consume();
                item.op = ListOp::And;
                list.items.push_back(item);
                item = {};
                item.pill = parse_pipeline();
            } else break;
        }
    }

    // Last item (no trailing operator)
    if (!item.pill.cmds.empty())
        list.items.push_back(item);

    return list;
}
