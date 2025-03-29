#pragma once

#include <string>
#include <vector>

namespace punkt {
struct Digraph;
}

namespace punkt::tokenizer {
struct Token {
    enum class Type {
        eos = -1,
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
        undirected_conn,
    };

    std::string_view m_value;
    Type m_type;

    Token(std::string_view value, Type type);

    bool operator==(const Token &other) const;
};

std::vector<Token> tokenize(Digraph &dg, std::string_view s);

class UnexpectedCharException final : std::exception {
    char m_c;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit UnexpectedCharException(char c);
};
}
