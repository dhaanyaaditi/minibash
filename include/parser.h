#pragma once
#include "common.h"
#include "lexer.h"
#include <vector>
#include <stdexcept>

/*  Parser: tokens → CmdList (AST)
 *
 *  Grammar (simplified):
 *    cmdlist  := pipeline ( ('&&' | '||' | ';' | '&') pipeline )*
 *    pipeline := ['!'] simplecmd ( '|' simplecmd )*
 *    simplecmd:= word+ redir*
 *    redir    := ('>' | '>>' | '<' | '<<') word
 */
class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)), pos_(0) {}

    CmdList parse();

private:
    std::vector<Token> tokens_;
    size_t             pos_;

    const Token& peek(size_t offset = 0) const;
    const Token& consume();
    bool at_end() const;

    Pipeline   parse_pipeline();
    SimpleCmd  parse_simple_cmd();
    Redirect   parse_redirect();
    Word       parse_word();
};
