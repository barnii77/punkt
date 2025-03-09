#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace dot::tokenizer {
struct Token {
    enum class Type {
        eos = -1,
        ident,
        string,
        kwd,
        lsq,
        rsq,
        lcurly,
        rcurly,
        equals,
        semicolon,
        comma,
        comment_start,
        arrow,
    };

    std::string_view m_value;
    Type m_type;

    Token(std::string_view value, Type type);

    bool operator==(const Token &other) const;
};

std::vector<Token> tokenize(std::string_view s);

class UnexpectedCharException final : std::exception {
    char m_c;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit UnexpectedCharException(char c);
};
}
