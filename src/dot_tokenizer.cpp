#include "punkt/dot_tokenizer.hpp"
#include "punkt/dot_keywords.hpp"

#include <unordered_set>
#include <cassert>

using namespace dot::tokenizer;

static std::unordered_set<std::string_view> keywords = {
    KWD_EDGE,
    KWD_NODE,
    KWD_GRAPH,
    KWD_DIGRAPH,
};

Token::Token(const std::string_view value, const Type type)
    : m_value(value), m_type(type) {
}

bool Token::operator==(const Token &other) const = default;

UnexpectedCharException::UnexpectedCharException(const char c)
    : m_c(c) {
}

const char *UnexpectedCharException::what() const noexcept {
    return (std::string("Unexpected char encountered: ") + m_c).c_str();
}

static std::string_view consumeIdent(std::string_view &s) {
    assert(!s.empty());
    size_t n = 0;
    while (n < s.length() && std::isalnum(s.at(n))) {
        n++;
    }

    const std::string_view out = s.substr(0, n);
    s = s.substr(n, s.length() - n);
    return out;
}

static bool isKwd(const std::string_view &s) {
    return keywords.contains(s);
}

std::vector<Token> dot::tokenizer::tokenize(std::string_view s) {
    std::vector<Token> out;

    while (!s.empty()) {
        size_t advance_by = 0;

        if (char c = s.front(); std::isalnum(c)) {
            std::string_view ident = consumeIdent(s);
            out.emplace_back(ident, isKwd(ident) ? Token::Type::kwd : Token::Type::ident);
        } else if (c == '[') {
            out.emplace_back("[", Token::Type::lsq);
            advance_by = 1;
        } else if (c == ']') {
            out.emplace_back("]", Token::Type::rsq);
            advance_by = 1;
        } else if (c == '{') {
            out.emplace_back("{", Token::Type::lcurly);
            advance_by = 1;
        } else if (c == '}') {
            out.emplace_back("}", Token::Type::rcurly);
            advance_by = 1;
        } else if (c == '=') {
            out.emplace_back("=", Token::Type::equals);
            advance_by = 1;
        } else if (c == ';') {
            out.emplace_back(";", Token::Type::semicolon);
            advance_by = 1;
        } else if (c == ',') {
            out.emplace_back(",", Token::Type::comma);
            advance_by = 1;
        } else if (c == '-') {
            // TODO handle undirected edges
            if (s.length() < 2 || (c = s.at(1)) != '>') {
                throw UnexpectedCharException(c);
            }
            out.emplace_back("->", Token::Type::arrow);
            advance_by = 2;
        } else if (c == '/') {
            if (s.length() < 2 || (c = s.at(1)) != '/') {
                throw UnexpectedCharException(c);
            }
            advance_by = 2;
            // advance until newline
            while (advance_by < s.length() && s.at(advance_by) != '\n') {
                advance_by++;
            }
            if (advance_by < s.length()) {
                // also consume newline
                advance_by++;
            }
        } else if (c == '"') {
            // handle strings
            advance_by = 1;
            // advance until end of string
            bool prev_was_backslash = false;
            while (advance_by < s.length() && (s.at(advance_by) != '"' || prev_was_backslash)) {
                prev_was_backslash = s.at(advance_by) == '\\';
                advance_by++;
            }
            if (advance_by < s.length()) {
                // also consume end quote
                advance_by++;
            } else {
                // unexpected EOS
                throw UnexpectedCharException(0);
            }
            assert(advance_by >= 2);
            const size_t n = advance_by - 2;  // remove quotation marks
            out.emplace_back(s.substr(1, n), Token::Type::string);
        } else if (std::isspace(c)) {
            advance_by = 1;
        } else {
            throw UnexpectedCharException(c);
        }

        s = s.substr(advance_by, s.length() - advance_by);
    }

    out.emplace_back("", Token::Type::eos);

    return out;
}
