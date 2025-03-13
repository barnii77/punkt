#include "punkt/dot.hpp"
#include "punkt/dot_tokenizer.hpp"
#include <gtest/gtest.h>

using namespace punkt;
using namespace punkt::tokenizer;

TEST(tokenizer, SimpleTest) {
    constexpr std::string_view test_input = "digraph G { node [color=red, style=dotted];A -> B;  C;}";
    Digraph dg;
    const std::vector<Token> tokens = tokenize(dg, test_input);

    ASSERT_EQ(tokens, std::vector({
        Token("digraph", Token::Type::kwd),
        Token("G", Token::Type::string),
        Token("{", Token::Type::lcurly),
        Token("node", Token::Type::kwd),
        Token("[", Token::Type::lsq),
        Token("color", Token::Type::string),
        Token("=", Token::Type::equals),
        Token("red", Token::Type::string),
        Token(",", Token::Type::comma),
        Token("style", Token::Type::string),
        Token("=", Token::Type::equals),
        Token("dotted", Token::Type::string),
        Token("]", Token::Type::rsq),
        Token(";", Token::Type::semicolon),
        Token("A", Token::Type::string),
        Token("->", Token::Type::arrow),
        Token("B", Token::Type::string),
        Token(";", Token::Type::semicolon),
        Token("C", Token::Type::string),
        Token(";", Token::Type::semicolon),
        Token("}", Token::Type::rcurly),
        Token("", Token::Type::eos),
    }));
}

TEST(tokenizer, StringTest) {
    constexpr std::string_view test_input = "\"Hello\\n\\\\world\" \"\"\"\t\\t\\n\n\"";
    Digraph dg;
    const std::vector<Token> tokens = tokenize(dg, test_input);

    ASSERT_EQ(tokens, std::vector({
        Token("Hello\n\\world", Token::Type::string),
        Token("", Token::Type::string),
        Token("        \n\n", Token::Type::string),
        Token("", Token::Type::eos),
    }));
}
