#include "punkt/dot.hpp"
#include "punkt/dot_tokenizer.hpp"
#include "punkt/dot_keywords.hpp"

#include <utility>
#include <ranges>
#include <algorithm>
#include <vector>
#include <span>
#include <cstdint>
#include <optional>
#include <cassert>

using namespace dot;

UnexpectedTokenException::UnexpectedTokenException(const tokenizer::Token &token)
    : m_token(token) {
}

const char *UnexpectedTokenException::what() const noexcept {
    return (std::string("Unexpected token: \"") + std::string(m_token.m_value) + "\" of type " + std::to_string(
                static_cast<uint64_t>(m_token.m_type))).c_str();
}

EdgeRenderAttrs::EdgeRenderAttrs()
    : m_is_visible(true) {
}

Edge::Edge(const std::string_view source, const std::string_view destination)
    : m_source(source), m_dest(destination) {
}

Edge::Edge(const std::string_view source, const std::string_view destination, Attrs attrs)
    : Edge(source, destination) {
    m_attrs = std::move(attrs);
}

NodeRenderAttrs::NodeRenderAttrs() = default;

Node::Node(const std::string_view name, Attrs attrs)
    : m_name(name), m_attrs(std::move(attrs)) {
}

static tokenizer::Token expectAndConsumeOneOf(std::span<tokenizer::Token> &tokens,
                                              const std::span<tokenizer::Token::Type> &types) {
    const tokenizer::Token &tok = tokens.front();
    if (std::ranges::find(types, tok.m_type) == types.end()) {
        throw UnexpectedTokenException(tok);
    }
    tokens = tokens.subspan(1, tokens.size() - 1);
    return tok;
}

static tokenizer::Token expectAndConsume(std::span<tokenizer::Token> &tokens, const tokenizer::Token::Type type,
                                         const std::optional<std::string_view> &value = std::nullopt) {
    const tokenizer::Token &tok = tokens.front();
    if (tok.m_type != type || value.has_value() && value.value() != tok.m_value) {
        throw UnexpectedTokenException(tok);
    }
    tokens = tokens.subspan(1, tokens.size() - 1);
    return tok;
}

static bool nextTokenIs(const std::span<tokenizer::Token> &tokens, const tokenizer::Token::Type type,
                        const std::optional<std::string_view> &value = std::nullopt) {
    assert(!tokens.empty());
    const tokenizer::Token &tok = tokens.front();
    return tok.m_type == type && (!value.has_value() || value.value() == tok.m_value);
}

static void implicitCreateNodeIfNotExists(Digraph &dg, const std::string_view name) {
    if (!dg.m_nodes.contains(name)) {
        dg.m_nodes.insert_or_assign(name, Node(name, {}));
    }
}

static Attrs consumeAttrs(std::span<tokenizer::Token> &tokens) {
    if (!nextTokenIs(tokens, tokenizer::Token::Type::lsq)) {
        return {};
    }

    Attrs attrs;
    expectAndConsume(tokens, tokenizer::Token::Type::lsq);

    while (!nextTokenIs(tokens, tokenizer::Token::Type::rsq)) {
        // parse key=value pair
        std::string_view key = expectAndConsume(tokens, tokenizer::Token::Type::ident).m_value;
        expectAndConsume(tokens, tokenizer::Token::Type::equals);
        static std::array value_expected_types = {tokenizer::Token::Type::ident, tokenizer::Token::Type::string};
        std::string_view value = expectAndConsumeOneOf(tokens, value_expected_types).m_value;
        attrs.insert_or_assign(key, value);
        if (nextTokenIs(tokens, tokenizer::Token::Type::comma)) {
            expectAndConsume(tokens, tokenizer::Token::Type::comma);
        } else if (!nextTokenIs(tokens, tokenizer::Token::Type::rsq)) {
            // got something that is not `,` or `]` after a style attribute, e.g. `[color=red, style=dotted >>}<<]`
            throw UnexpectedTokenException(tokens.front());
        }
    }

    expectAndConsume(tokens, tokenizer::Token::Type::rsq);
    return attrs;
}

// merge attrs such that attrs in b shadow attrs in a
static void mergeAttrs(const Attrs &a, Attrs &b) {
    b.insert(a.begin(), a.end());
}

static void consumeStatementAndUpdateDigraph(Digraph &dg, std::span<tokenizer::Token> &tokens) {
    // TODO handle clusters and sub-graphs here
    static std::array expected_token_types = {tokenizer::Token::Type::ident, tokenizer::Token::Type::kwd};
    tokenizer::Token a = expectAndConsumeOneOf(tokens, expected_token_types);
    if (a.m_type == tokenizer::Token::Type::kwd) {
        // handle special keywords: for now only `node` and `edge` (which set default attrs)
        if (const std::string_view &kwd = a.m_value; kwd == KWD_NODE) {
            Attrs new_attrs = consumeAttrs(tokens);
            mergeAttrs(dg.default_node_attrs, new_attrs);
            dg.default_node_attrs = std::move(new_attrs);
        } else if (kwd == KWD_EDGE) {
            Attrs new_attrs = consumeAttrs(tokens);
            mergeAttrs(dg.default_edge_attrs, new_attrs);
            dg.default_edge_attrs = std::move(new_attrs);
        } else {
            throw UnexpectedTokenException(a);
        }
    } else if (nextTokenIs(tokens, tokenizer::Token::Type::arrow)) {
        // handle edge declaration(s)
        implicitCreateNodeIfNotExists(dg, a.m_value);
        std::vector<Edge> new_edges;

        do {
            expectAndConsume(tokens, tokenizer::Token::Type::arrow);
            const tokenizer::Token b = expectAndConsume(tokens, tokenizer::Token::Type::ident);
            implicitCreateNodeIfNotExists(dg, b.m_value);
            new_edges.emplace_back(a.m_value, b.m_value);
            a = b;
        } while (nextTokenIs(tokens, tokenizer::Token::Type::arrow));

        // parse attrs
        Attrs attrs = consumeAttrs(tokens);
        // handle defaults set by `node [...];`
        mergeAttrs(dg.default_edge_attrs, attrs);

        // assign attrs
        for (Edge &e: new_edges) {
            e.m_attrs = attrs;
        }

        // insert edges
        for (Edge &e: new_edges) {
            dg.m_nodes.at(e.m_source).m_outgoing.emplace_back(std::move(e));
        }
    } else {
        // handle node declaration
        Attrs attrs = consumeAttrs(tokens);
        // handle defaults set by `node [...];`
        mergeAttrs(dg.default_node_attrs, attrs);

        dg.m_nodes.insert_or_assign(a.m_value, Node(a.m_value, attrs));
    }

    // every statement ends with a semicolon
    expectAndConsume(tokens, tokenizer::Token::Type::semicolon);
}

Digraph::Digraph() = default;

Digraph::Digraph(std::string source)
    : m_source(std::move(source)) {
    std::vector<tokenizer::Token> tokens_vec = tokenizer::tokenize(m_source);
    std::span tokens = tokens_vec;

    expectAndConsume(tokens, tokenizer::Token::Type::kwd, KWD_DIGRAPH);
    m_name = expectAndConsume(tokens, tokenizer::Token::Type::ident).m_value;
    expectAndConsume(tokens, tokenizer::Token::Type::lcurly);
    while (!tokens.empty() && !nextTokenIs(tokens, tokenizer::Token::Type::rcurly)) {
        consumeStatementAndUpdateDigraph(*this, tokens);
    }
    expectAndConsume(tokens, tokenizer::Token::Type::rcurly);
    // everything after the first graph is ignored
}

Digraph::Digraph(const std::string_view source)
    : Digraph(std::string(source)) {
}

void Digraph::populateIngoingNodesVectors() {
    // populate m_ingoing for every node
    for (auto &node: std::views::values(m_nodes)) {
        for (auto &edge: node.m_outgoing) {
            m_nodes.at(edge.m_dest).m_ingoing.emplace_back(edge);
        }
    }
}

void Digraph::deleteIngoingNodesVectors() {
    // clear m_ingoing for every node
    for (auto &node: std::views::values(m_nodes)) {
        node.m_ingoing.clear();
    }
}
