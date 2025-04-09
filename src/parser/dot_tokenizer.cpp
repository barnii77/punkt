#include "punkt/dot.hpp"
#include "punkt/dot_tokenizer.hpp"
#include "punkt/dot_constants.hpp"

#include <unordered_set>
#include <cassert>

using namespace punkt;
using namespace punkt::tokenizer;

static std::unordered_set<std::string_view> keywords = {
    KWD_EDGE,
    KWD_NODE,
    KWD_GRAPH,
    KWD_DIGRAPH,
    KWD_SUBGRAPH,
};

Token::Token(const std::string_view value, const Type type)
    : m_value(value), m_type(type) {
}

bool Token::operator==(const Token &other) const = default;

UnexpectedCharException::UnexpectedCharException(const char c)
    : m_c(c) {
}

const char *UnexpectedCharException::what() const noexcept {
    const std::string msg = std::string("Unexpected char encountered: ") + m_c;
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

static std::string_view consumeIdent(std::string_view &s) {
    assert(!s.empty());
    size_t n = 0;
    while (n < s.length() && (std::isalnum(s.at(n)) || s.at(n) == '_' || s.at(n) == '.')) {
        n++;
    }

    const std::string_view out = s.substr(0, n);
    s = s.substr(n, s.length() - n);
    return out;
}

static bool isKwd(const std::string_view &s) {
    return keywords.contains(s);
}

std::vector<Token> punkt::tokenizer::tokenize(Digraph &dg, std::string_view s) {
    std::vector<Token> out;

    while (!s.empty()) {
        size_t advance_by = 0;

        if (char c = s.front(); std::isalnum(c) || c == '_' || c == '.') {
            std::string_view ident = consumeIdent(s);
            out.emplace_back(ident, isKwd(ident) ? Token::Type::kwd : Token::Type::string);
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
            c = s.length() < 2 ? '\0' : s.at(1);
            if (c == '>') {
                out.emplace_back("->", Token::Type::arrow);
            } else if (c == '-') {
                out.emplace_back("--", Token::Type::undirected_conn);
            } else {
                throw UnexpectedCharException(c);
            }
            advance_by = 2;
        } else if (c == '/' || c == '#') {
            if (c == '/' && (s.length() < 2 || (c = s.at(1)) != '/')) {
                throw UnexpectedCharException(c);
            }
            advance_by = c == '/' ? 2 : 1;
            // advance until newline
            while (advance_by < s.length() && s.at(advance_by) != '\n') {
                advance_by++;
            }
            if (advance_by < s.length()) {
                // also consume newline
                advance_by++;
            }
        } else if (c == '"') {
            // handle strings (which are Token::Type::ident)
            advance_by = 1;
            // advance until end of string
            bool prev_was_backslash = false;
            std::vector<char> accum;
            bool has_special_chars = false;
            while (advance_by < s.length() && ((c = s.at(advance_by)) != '"' || prev_was_backslash)) {
                if (prev_was_backslash && c == 'n') {
                    accum.emplace_back('\n');
                    has_special_chars = true;
                } else if (prev_was_backslash && c == '\\') {
                    accum.emplace_back('\\');
                    has_special_chars = true;
                } else if (prev_was_backslash && c == 't' || c == '\t') {
                    // lazy tab handling
                    for (size_t i = 0; i < 4; i++) {
                        accum.emplace_back(' ');
                    }
                } else if (c != '\\') {
                    accum.emplace_back(c);
                }
                prev_was_backslash = c == '\\';
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
            if (has_special_chars) {
                dg.m_generated_sources.emplace_front(accum.begin(), accum.end());
                out.emplace_back(dg.m_generated_sources.front(), Token::Type::string);
            } else {
                out.emplace_back(s.substr(1, n), Token::Type::string);
            }
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
