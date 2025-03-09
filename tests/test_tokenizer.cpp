#include "punkt/dot_tokenizer.hpp"
#include <gtest/gtest.h>

using namespace dot::tokenizer;

TEST(tokenizer, SimpleTest) {
    constexpr std::string_view test_input = "digraph G { node [color=red, style=dotted];A -> B;  C;}";
    const std::vector<Token> tokens = tokenize(test_input);

    ASSERT_EQ(tokens, std::vector({
        Token("digraph", Token::Type::kwd),
        Token("G", Token::Type::ident),
        Token("{", Token::Type::lcurly),
        Token("node", Token::Type::kwd),
        Token("[", Token::Type::lsq),
        Token("color", Token::Type::ident),
        Token("=", Token::Type::equals),
        Token("red", Token::Type::ident),
        Token(",", Token::Type::comma),
        Token("style", Token::Type::ident),
        Token("=", Token::Type::equals),
        Token("dotted", Token::Type::ident),
        Token("]", Token::Type::rsq),
        Token(";", Token::Type::semicolon),
        Token("A", Token::Type::ident),
        Token("->", Token::Type::arrow),
        Token("B", Token::Type::ident),
        Token(";", Token::Type::semicolon),
        Token("C", Token::Type::ident),
        Token(";", Token::Type::semicolon),
        Token("}", Token::Type::rcurly),
        Token("", Token::Type::eos),
    }));
}
