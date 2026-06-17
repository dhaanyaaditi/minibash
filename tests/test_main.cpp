#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/environment.h"
#include "../include/history.h"

#define TEST(name) void test_##name()
#define RUN(name)  do { test_##name(); std::cout << "  PASS  " #name "\n"; } while(0)

// ── Lexer ─────────────────────────────────────────────────────────────────────
TEST(lex_simple_word) {
    Lexer l("echo hello");
    auto toks = l.tokenise();
    assert(toks[0].type == TokenType::Word && toks[0].value == "echo");
    assert(toks[1].type == TokenType::Word && toks[1].value == "hello");
    assert(toks[2].type == TokenType::EndOfInput);
}

TEST(lex_pipe) {
    Lexer l("ls | grep txt");
    auto toks = l.tokenise();
    assert(toks[0].value == "ls");
    assert(toks[1].type  == TokenType::Pipe);
    assert(toks[2].value == "grep");
    assert(toks[3].value == "txt");
}

TEST(lex_and_or) {
    Lexer l("cmd1 && cmd2 || cmd3");
    auto toks = l.tokenise();
    assert(toks[1].type == TokenType::And);
    assert(toks[3].type == TokenType::Or);
}

TEST(lex_redirections) {
    Lexer l("cmd > out.txt 2>> err.txt < in.txt");
    auto toks = l.tokenise();
    assert(toks[1].type == TokenType::RedirOut);
    assert(toks[2].value == "out.txt");
    assert(toks[4].type == TokenType::RedirOutApp);
    assert(toks[6].type == TokenType::RedirIn);
}

TEST(lex_single_quoted) {
    Lexer l("echo 'hello world'");
    auto toks = l.tokenise();
    assert(toks[1].value == "hello world");
}

TEST(lex_double_quoted) {
    Lexer l("echo \"hello world\"");
    auto toks = l.tokenise();
    assert(toks[1].value == "hello world");
}

TEST(lex_backslash_escape) {
    Lexer l("echo hello\\ world");
    auto toks = l.tokenise();
    assert(toks[1].value == "hello world");
}

TEST(lex_comment) {
    Lexer l("echo hi # this is a comment");
    auto toks = l.tokenise();
    assert(toks[0].value == "echo");
    assert(toks[1].value == "hi");
    assert(toks[2].type  == TokenType::EndOfInput);
}

TEST(lex_semicolon) {
    Lexer l("echo a; echo b");
    auto toks = l.tokenise();
    assert(toks[2].type == TokenType::Semi);
}

TEST(lex_background) {
    Lexer l("sleep 5 &");
    auto toks = l.tokenise();
    assert(toks[2].type == TokenType::Amp);
}

// ── Parser ────────────────────────────────────────────────────────────────────
TEST(parse_simple_cmd) {
    Lexer  l("echo hello world");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items.size() == 1);
    auto& pl = list.items[0].pill;
    assert(pl.cmds.size() == 1);
    assert(pl.cmds[0].words[0].raw == "echo");
    assert(pl.cmds[0].words[1].raw == "hello");
    assert(pl.cmds[0].words[2].raw == "world");
}

TEST(parse_pipeline) {
    Lexer  l("ls | grep .cpp | wc -l");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items[0].pill.cmds.size() == 3);
}

TEST(parse_redirect) {
    Lexer  l("echo hi > out.txt");
    Parser p(l.tokenise());
    auto list = p.parse();
    auto& cmd = list.items[0].pill.cmds[0];
    assert(cmd.redirs.size() == 1);
    assert(cmd.redirs[0].type   == RedirType::RedirOut);
    assert(cmd.redirs[0].target == "out.txt");
}

TEST(parse_cmd_list) {
    Lexer  l("echo a; echo b; echo c");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items.size() == 3);
}

TEST(parse_negate) {
    Lexer  l("! false");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items[0].pill.negate == true);
}

// ── Environment ───────────────────────────────────────────────────────────────
TEST(env_set_get) {
    Environment e;
    e.set("FOO", "bar");
    assert(e.get("FOO") == "bar");
    assert(e.has("FOO"));
}

TEST(env_unset) {
    Environment e;
    e.set("X", "1");
    e.unset("X");
    assert(!e.has("X"));
    assert(e.get("X").empty());
}

TEST(env_expand_simple) {
    Environment e;
    e.set("NAME", "aditi");
    assert(e.expand("hello $NAME") == "hello aditi");
    assert(e.expand("${NAME}!") == "aditi!");
}

TEST(env_expand_special) {
    Environment e;
    e.set_last_status(42);
    assert(e.expand("$?") == "42");
}

TEST(env_expand_no_var) {
    Environment e;
    // Unset variable expands to empty string
    assert(e.expand("$UNSET_VAR_XYZ") == "");
}

TEST(env_expand_tilde) {
    Environment e;
    e.set("HOME", "/home/aditi");
    std::string r = e.expand("~/projects");
    assert(r == "/home/aditi/projects");
}

TEST(env_export) {
    Environment e;
    e.set("MY_VAR", "hello");
    assert(!e.is_exported("MY_VAR"));
    e.export_var("MY_VAR");
    assert(e.is_exported("MY_VAR"));
}

TEST(env_find_in_path) {
    Environment e;
    // 'ls' should exist on any Linux system
    auto p = e.find_in_path("ls");
    assert(p.has_value());
    assert(p->find("ls") != std::string::npos);
}

TEST(env_find_in_path_missing) {
    Environment e;
    auto p = e.find_in_path("definitely_not_a_real_command_xyz");
    assert(!p.has_value());
}

// ── History ───────────────────────────────────────────────────────────────────
TEST(history_add_get) {
    History h;
    h.add("echo hello");
    h.add("ls -la");
    assert(h.size() == 2);
    assert(h.at(1) == "echo hello");
    assert(h.at(2) == "ls -la");
    assert(h.last() == "ls -la");
}

TEST(history_no_duplicates) {
    History h;
    h.add("ls");
    h.add("ls");
    h.add("ls");
    assert(h.size() == 1);
}

TEST(history_expand_bang_bang) {
    History h;
    h.add("echo hello");
    std::string r = h.expand("!!");
    assert(r == "echo hello");
}

TEST(history_expand_bang_n) {
    History h;
    h.add("echo first");
    h.add("echo second");
    std::string r = h.expand("!1");
    assert(r == "echo first");
}

TEST(history_expand_bang_string) {
    History h;
    h.add("git status");
    h.add("ls -la");
    std::string r = h.expand("!git");
    assert(r == "git status");
}

TEST(history_search) {
    History h;
    h.add("make build");
    h.add("make test");
    h.add("echo hello");
    auto r = h.search("make");
    assert(r.has_value());
    assert(*r == "make test"); // most recent
}

// ── Integration: Lexer + Parser ───────────────────────────────────────────────
TEST(integrate_pipe_redir) {
    Lexer  l("cat file.txt | grep 'hello' > out.txt");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items.size() == 1);
    auto& pl = list.items[0].pill;
    assert(pl.cmds.size() == 2);
    assert(pl.cmds[0].words[0].raw == "cat");
    assert(pl.cmds[1].words[0].raw == "grep");
    assert(pl.cmds[1].redirs.size() == 1);
    assert(pl.cmds[1].redirs[0].target == "out.txt");
}

TEST(integrate_multiline_semicolons) {
    Lexer  l("cd /tmp; ls; pwd");
    Parser p(l.tokenise());
    auto list = p.parse();
    assert(list.items.size() == 3);
    assert(list.items[0].pill.cmds[0].words[0].raw == "cd");
    assert(list.items[1].pill.cmds[0].words[0].raw == "ls");
    assert(list.items[2].pill.cmds[0].words[0].raw == "pwd");
}

int main() {
    std::cout << "=== minibash test suite ===\n";

    std::cout << "\n[lexer]\n";
    RUN(lex_simple_word);
    RUN(lex_pipe);
    RUN(lex_and_or);
    RUN(lex_redirections);
    RUN(lex_single_quoted);
    RUN(lex_double_quoted);
    RUN(lex_backslash_escape);
    RUN(lex_comment);
    RUN(lex_semicolon);
    RUN(lex_background);

    std::cout << "\n[parser]\n";
    RUN(parse_simple_cmd);
    RUN(parse_pipeline);
    RUN(parse_redirect);
    RUN(parse_cmd_list);
    RUN(parse_negate);

    std::cout << "\n[environment]\n";
    RUN(env_set_get);
    RUN(env_unset);
    RUN(env_expand_simple);
    RUN(env_expand_special);
    RUN(env_expand_no_var);
    RUN(env_expand_tilde);
    RUN(env_export);
    RUN(env_find_in_path);
    RUN(env_find_in_path_missing);

    std::cout << "\n[history]\n";
    RUN(history_add_get);
    RUN(history_no_duplicates);
    RUN(history_expand_bang_bang);
    RUN(history_expand_bang_n);
    RUN(history_expand_bang_string);
    RUN(history_search);

    std::cout << "\n[integration]\n";
    RUN(integrate_pipe_redir);
    RUN(integrate_multiline_semicolons);

    std::cout << "\n✓ All tests passed.\n";
    return 0;
}
