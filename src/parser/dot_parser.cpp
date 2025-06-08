#include "punkt/dot.hpp"
#include "punkt/dot_tokenizer.hpp"
#include "punkt/utils/utils.hpp"
#include "punkt/utils/int_types.hpp"
#include "punkt/dot_constants.hpp"

#include <utility>
#include <array>
#include <ranges>
#include <algorithm>
#include <vector>
#include <span>
#include <optional>
#include <cassert>
#include <stack>

using namespace punkt;

IllegalAttributeException::IllegalAttributeException(std::string attr, std::string value)
    : m_attr(std::move(attr)), m_value(std::move(value)) {
}

const char *IllegalAttributeException::what() const noexcept {
    const std::string msg = std::string("Invalid attribute: \"") + std::string(m_attr) + "\" with value \"" + m_value +
                            "\"";
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

ReservedIdentifierException::ReservedIdentifierException(std::string name)
    : m_name(std::move(name)) {
}

const char *ReservedIdentifierException::what() const noexcept {
    const std::string msg = std::string("Usage of reserved identifier \"") + m_name + "\"";
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

UnexpectedTokenException::UnexpectedTokenException(const tokenizer::Token &token)
    : m_token(token) {
}

const char *UnexpectedTokenException::what() const noexcept {
    const std::string msg = std::string("Unexpected token: \"") + std::string(m_token.m_value) + "\" of type " +
                            std::to_string(static_cast<uint64_t>(m_token.m_type));
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
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

static void implicitCreateNodeIfNotExists(Digraph &dg, const std::string_view name, const Attrs &attrs) {
    if (!dg.m_nodes.contains(name)) {
        dg.m_nodes.insert_or_assign(name, Node(name, attrs));
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
        std::string_view key = expectAndConsume(tokens, tokenizer::Token::Type::string).m_value;
        expectAndConsume(tokens, tokenizer::Token::Type::equals);
        std::string_view value = expectAndConsume(tokens, tokenizer::Token::Type::string).m_value;
        attrs.insert_or_assign(key, value);
        if (nextTokenIs(tokens, tokenizer::Token::Type::comma)) {
            expectAndConsume(tokens, tokenizer::Token::Type::comma);
        } else if (!nextTokenIs(tokens, tokenizer::Token::Type::rsq) && !nextTokenIs(
                       tokens, tokenizer::Token::Type::string)) {
            // Got something that is not `,` or `]` after a style attribute, e.g. `[color=red, style=dotted >>}<<]`.
            // Now, there is the special case where we have an attr list without comma separation, which doesn't throw.
            throw UnexpectedTokenException(tokens.front());
        }
    }

    expectAndConsume(tokens, tokenizer::Token::Type::rsq);
    return attrs;
}

// merge attrs such that attrs in b shadow attrs in a
static void mergeAttrsInto(const Attrs &a, Attrs &b) {
    b.insert(a.begin(), a.end());
}

static void validateNodeName(const std::string_view &name) {
    if (name.starts_with("@")) {
        throw ReservedIdentifierException(std::string(name));
    }
}

static void validateAttrs(const Attrs &attrs) {
    for (const std::string_view &attr: std::views::keys(attrs)) {
        if (attr.starts_with("@")) {
            throw ReservedIdentifierException(std::string(attr));
        }
    }
}

static std::string_view consumeGraphSourceAndUpdateDigraph(Digraph &dg, std::span<tokenizer::Token> &tokens);

static void consumeStatementAndUpdateDigraph(Digraph &dg, std::span<tokenizer::Token> &tokens) {
    assert(!tokens.empty());
    static std::array expected_token_types = {
        tokenizer::Token::Type::string, tokenizer::Token::Type::kwd, tokenizer::Token::Type::lcurly
    };
    std::span<tokenizer::Token> unconsumed_tokens = tokens;

    if (tokenizer::Token a = expectAndConsumeOneOf(tokens, expected_token_types);
        a.m_type == tokenizer::Token::Type::kwd) {
        // handle special keywords: for now only `node` and `edge` (which set default attrs)
        if (const std::string_view &kwd = a.m_value; kwd == KWD_NODE) {
            Attrs new_attrs = consumeAttrs(tokens);
            mergeAttrsInto(dg.m_default_node_attrs, new_attrs);
            dg.m_default_node_attrs = std::move(new_attrs);
        } else if (kwd == KWD_EDGE) {
            Attrs new_attrs = consumeAttrs(tokens);
            mergeAttrsInto(dg.m_default_edge_attrs, new_attrs);
            dg.m_default_edge_attrs = std::move(new_attrs);
        } else if (kwd == KWD_GRAPH) {
            Attrs new_attrs = consumeAttrs(tokens);
            mergeAttrsInto(dg.m_attrs, new_attrs);
            dg.m_attrs = std::move(new_attrs);
        } else if (kwd == KWD_SUBGRAPH) {
            // save the defaults
            const Attrs default_node_attrs = dg.m_default_node_attrs;
            const Attrs default_edge_attrs = dg.m_default_edge_attrs;
            // parse subgraph
            consumeGraphSourceAndUpdateDigraph(dg, unconsumed_tokens);
            tokens = unconsumed_tokens;
            // restore the defaults
            dg.m_default_node_attrs = default_node_attrs;
            dg.m_default_edge_attrs = default_edge_attrs;
        } else {
            throw UnexpectedTokenException(a);
        }
    } else if (a.m_type == tokenizer::Token::Type::lcurly) {
        // constraint, e.g. `{ rank=min; A B C; }`
        expectAndConsume(tokens, tokenizer::Token::Type::string, "rank");
        expectAndConsume(tokens, tokenizer::Token::Type::equals);
        const tokenizer::Token ty = expectAndConsume(tokens, tokenizer::Token::Type::string);
        if (ty.m_value != "min" && ty.m_value != "max" && ty.m_value != "same" && ty.m_value != "sink" &&
            ty.m_value != "source") {
            throw UnexpectedTokenException(ty);
        }
        std::vector<std::string_view> constrained_nodes;
        expectAndConsume(tokens, tokenizer::Token::Type::semicolon);

        // parse nodes
        while (!nextTokenIs(tokens, tokenizer::Token::Type::rcurly)) {
            tokenizer::Token node_tok = expectAndConsume(tokens, tokenizer::Token::Type::string);
            validateNodeName(node_tok.m_value);
            // handle node declaration
            Attrs attrs = consumeAttrs(tokens);
            validateAttrs(attrs);
            // handle defaults set by `node [...];`
            mergeAttrsInto(dg.m_default_node_attrs, attrs);
            if (nextTokenIs(tokens, tokenizer::Token::Type::semicolon)) {
                expectAndConsume(tokens, tokenizer::Token::Type::semicolon);
            }
            dg.m_nodes.insert_or_assign(node_tok.m_value, Node(node_tok.m_value, attrs));

            constrained_nodes.emplace_back(node_tok.m_value);
        }
        expectAndConsume(tokens, tokenizer::Token::Type::rcurly);
        for (const auto &node_name: constrained_nodes) {
            implicitCreateNodeIfNotExists(dg, node_name, dg.m_default_node_attrs);
            Node &node = dg.m_nodes.at(node_name);
            std::string constraints = std::string(getAttrOrDefault(node.m_attrs, "@constraints", "")) +
                                      std::to_string(dg.m_rank_constraints.size()) + ";";
            dg.m_referenced_sources.emplace_front(std::move(constraints));
            node.m_attrs.insert_or_assign("@constraints", dg.m_referenced_sources.front());
        }
        dg.m_rank_constraints.emplace_back(ty.m_value, std::move(constrained_nodes));
    } else if (nextTokenIs(tokens, tokenizer::Token::Type::arrow) ||
               nextTokenIs(tokens, tokenizer::Token::Type::undirected_conn)) {
        validateNodeName(a.m_value);
        // handle edge declaration(s)
        implicitCreateNodeIfNotExists(dg, a.m_value, dg.m_default_node_attrs);
        std::vector<Edge> new_edges;
        std::vector<bool> edges_undirected;

        do {
            static std::array expected_connection_tokens = {
                tokenizer::Token::Type::arrow, tokenizer::Token::Type::undirected_conn
            };
            tokenizer::Token conn_tok = expectAndConsumeOneOf(tokens, expected_connection_tokens);
            edges_undirected.emplace_back(conn_tok.m_type == tokenizer::Token::Type::undirected_conn);
            const tokenizer::Token b = expectAndConsume(tokens, tokenizer::Token::Type::string);
            implicitCreateNodeIfNotExists(dg, b.m_value, dg.m_default_node_attrs);
            new_edges.emplace_back(a.m_value, b.m_value);
            a = b;
        } while (nextTokenIs(tokens, tokenizer::Token::Type::arrow));

        // parse attrs
        Attrs attrs = consumeAttrs(tokens);
        validateAttrs(attrs);
        // handle defaults set by `node [...];`
        mergeAttrsInto(dg.m_default_edge_attrs, attrs);

        // assign attrs
        for (size_t i = 0; i < new_edges.size(); i++) {
            Edge &e = new_edges[i];
            if (edges_undirected[i]) {
                // make copy and insert dir=none (which makes it undirected)
                attrs = Attrs(attrs);
                attrs.insert_or_assign("dir", "none");
                e.m_attrs = std::move(attrs);
            } else {
                e.m_attrs = attrs;
            }
        }

        // insert edges
        for (Edge &e: new_edges) {
            dg.m_nodes.at(e.m_source).m_outgoing.emplace_back(std::move(e));
        }
    } else if (nextTokenIs(tokens, tokenizer::Token::Type::equals)) {
        // handle graph attribute
        expectAndConsume(tokens, tokenizer::Token::Type::equals);
        auto v = expectAndConsume(tokens, tokenizer::Token::Type::string);
        dg.m_attrs.insert_or_assign(a.m_value, v.m_value);
    } else {
        validateNodeName(a.m_value);
        // handle node declaration
        Attrs attrs = consumeAttrs(tokens);
        validateAttrs(attrs);
        // handle defaults set by `node [...];`
        mergeAttrsInto(dg.m_default_node_attrs, attrs);

        dg.m_nodes.insert_or_assign(a.m_value, Node(a.m_value, attrs));
    }

    // every statement should end with a semicolon (we don't crash if it doesn't though)
    if (nextTokenIs(tokens, tokenizer::Token::Type::semicolon)) {
        expectAndConsume(tokens, tokenizer::Token::Type::semicolon);
    }
}

std::string concatTokens(const std::span<tokenizer::Token> tokens) {
    size_t total_size = 0;
    for (const auto &token: tokens) {
        total_size += token.m_value.size();
    }

    std::string result;
    result.reserve(total_size);
    for (const auto &token: tokens) {
        result += token.m_value;
    }

    return result;
}

static bool isLeftParen(const tokenizer::Token &token) {
    return token.m_type == tokenizer::Token::Type::lcurly || token.m_type == tokenizer::Token::Type::lsq;
}

static bool isRightParen(const tokenizer::Token &token) {
    return token.m_type == tokenizer::Token::Type::rcurly || token.m_type == tokenizer::Token::Type::rsq;
}

static bool isMatchingParenPair(const tokenizer::Token &opening, const tokenizer::Token &closing) {
    return opening.m_type == tokenizer::Token::Type::lcurly && closing.m_type == tokenizer::Token::Type::rcurly ||
           opening.m_type == tokenizer::Token::Type::lsq && closing.m_type == tokenizer::Token::Type::rsq;
}

static bool containsClusterTrueAttr(const std::span<tokenizer::Token> tokens) {
    return false;

    // TODO clusters are currently not supported... this is WIP code (probably dead forever though)
    std::stack<tokenizer::Token> parens;
    tokenizer::Token prev_token{"", tokenizer::Token::Type::eos};
    tokenizer::Token prev_prev_token{"", tokenizer::Token::Type::eos};
    for (const auto &token: tokens) {
        if (isLeftParen(token)) {
            parens.push(token);
        } else if (isRightParen(token)) {
            if (const auto &top = parens.top(); !isMatchingParenPair(top, token)) {
                throw UnexpectedTokenException(token);
            }
            parens.pop();
        }

        if (parens.empty() &&
            token.m_type == tokenizer::Token::Type::string && caseInsensitiveEquals(token.m_value, "true") &&
            prev_token.m_type == tokenizer::Token::Type::equals &&
            prev_prev_token.m_type == tokenizer::Token::Type::string && prev_prev_token.m_value == "cluster") {
            // Found the token sequence `cluster` -> `=` -> `true` outside any parentheses, i.e. it is a cluster level
            // attr. I will assume that it was used in a syntactically correct way. Otherwise, the cluster parser will
            // throw anyway.
            return true;
        }

        prev_prev_token = prev_token;
        prev_token = token;
    }

    return false;
}

static void leakParentNodesInto(Digraph &cluster_dg, const Digraph &dg) {
    for (const Node &node: std::views::values(dg.m_nodes)) {
        if (cluster_dg.m_nodes.contains(node.m_name)) {
            continue;
        }
        Attrs leaked_attrs{{"@link", "parent"}, {"@type", "link"}};
        Node leaked_node{node.m_name, std::move(leaked_attrs)};
        cluster_dg.m_nodes.insert_or_assign(node.m_name, std::move(leaked_node));
    }
}

static void leakClusterNodesInto(Digraph &dg, const Digraph &cluster_dg) {
    for (const Node &cluster_node: std::views::values(cluster_dg.m_nodes)) {
        if (dg.m_nodes.contains(cluster_node.m_name)) {
            continue;
        }
        Attrs leaked_attrs{
            {"@link", "cluster_" + std::to_string(dg.m_cluster_order.at(cluster_dg.m_name))}, {"@type", "link"}
        };
        Node leaked_node{cluster_node.m_name, std::move(leaked_attrs)};
        dg.m_nodes.insert_or_assign(cluster_node.m_name, std::move(leaked_node));
    }
}

static std::string_view consumeGraphSourceAndUpdateDigraph(Digraph &dg, std::span<tokenizer::Token> &tokens) {
    const auto og_tokens = tokens;
    if (const auto tok = expectAndConsume(tokens, tokenizer::Token::Type::kwd);
        tok.m_value != KWD_DIGRAPH && tok.m_value != KWD_SUBGRAPH && tok.m_value != KWD_GRAPH) {
        throw UnexpectedTokenException(tok);
    }

    std::string_view name;
    if (nextTokenIs(tokens, tokenizer::Token::Type::string)) {
        name = expectAndConsume(tokens, tokenizer::Token::Type::string).m_value;
    }
    expectAndConsume(tokens, tokenizer::Token::Type::lcurly);

    // check if it is a cluster
    if (name.starts_with("cluster_") || containsClusterTrueAttr(tokens)) {
        Digraph *cluster_dg;
        if (dg.m_clusters.contains(name)) {
            cluster_dg = &dg.m_clusters[name];
        } else {
            dg.m_cluster_order.insert_or_assign(name, dg.m_clusters.size());
            dg.m_clusters.insert_or_assign(name, Digraph{});
            cluster_dg = &dg.m_clusters[name];
        }
        tokens = og_tokens;
        leakParentNodesInto(*cluster_dg, dg);
        cluster_dg->constructFromTokens(tokens);
        leakClusterNodesInto(dg, *cluster_dg);
        return name;
    }

    while (!tokens.empty() && !nextTokenIs(tokens, tokenizer::Token::Type::rcurly)) {
        consumeStatementAndUpdateDigraph(dg, tokens);
    }
    expectAndConsume(tokens, tokenizer::Token::Type::rcurly);

    // everything after the graph is ignored
    return name;
}

Digraph::Digraph() = default;

void Digraph::constructFromTokens(std::span<tokenizer::Token> &tokens) {
    m_name = consumeGraphSourceAndUpdateDigraph(*this, tokens);
    const std::string_view rank_dir = getAttrOrDefault(m_attrs, "rankdir", "TB");
    m_render_attrs.m_rank_dir = RankDirConfig{
        caseInsensitiveEquals(rank_dir, "LR") || caseInsensitiveEquals(rank_dir, "RL"),
        caseInsensitiveEquals(rank_dir, "RL") || caseInsensitiveEquals(rank_dir, "BT")
    };
}

Digraph::Digraph(std::string source) {
    m_referenced_sources.emplace_front(std::move(source));
    auto tokens_vec = tokenizer::tokenize(*this, m_referenced_sources.front());
    std::span tokens = tokens_vec;
    constructFromTokens(tokens);
}

Digraph::Digraph(const std::string_view source)
    : Digraph(std::string(source)) {
}

void Digraph::update(std::string_view extra_source) {
    m_referenced_sources.emplace_front(extra_source);
    extra_source = m_referenced_sources.front();
    auto tokens_vec = tokenizer::tokenize(*this, extra_source);
    std::span tokens = tokens_vec;
    consumeGraphSourceAndUpdateDigraph(*this, tokens);
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
