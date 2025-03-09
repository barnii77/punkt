#pragma once

#include "punkt/dot_tokenizer.hpp"

#include <string>
#include <forward_list>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace dot {
using Attrs = std::unordered_map<std::string_view, std::string_view>;

struct EdgeRenderAttrs {
    bool m_is_visible;

    explicit EdgeRenderAttrs();
};

struct Edge {
    std::string_view m_source;
    std::string_view m_dest;
    Attrs m_attrs;
    EdgeRenderAttrs m_render_attrs{};

    Edge(std::string_view source, std::string_view destination);

    Edge(std::string_view source, std::string_view destination, Attrs attrs);
};

struct NodeRenderAttrs {
    size_t m_rank{};
    bool m_is_ghost{};

    explicit NodeRenderAttrs();
};

struct Node {
    std::string_view m_name;
    std::vector<std::reference_wrapper<Edge> > m_ingoing;
    std::vector<Edge> m_outgoing;
    Attrs m_attrs;
    NodeRenderAttrs m_render_attrs{};

    Node(std::string_view name, Attrs attrs);
};

struct Digraph {
    // Digraph takes ownership of the source for safety because there are string_view's to the source everywhere
    std::string m_source;
    std::forward_list<std::string> m_generated_sources;
    std::string_view m_name;
    Attrs m_default_node_attrs;
    Attrs m_default_edge_attrs;
    std::unordered_map<std::string_view, Node> m_nodes;
    std::vector<size_t> m_rank_counts;
    std::vector<std::vector<std::string_view>> m_per_rank_orderings;
    std::vector<std::unordered_map<std::string_view, size_t>> m_per_rank_orderings_index;
    size_t m_n_ghost_nodes{};

    explicit Digraph();

    // parses digraph source
    explicit Digraph(std::string source);

    explicit Digraph(std::string_view source);

    void preprocess();

    void computeRanks();

    void insertGhostNodes();

    void deleteIngoingNodesVectors();

    void populateIngoingNodesVectors();

    void computeHorizontalOrderings();

private:
    void swapNodesOnRank(std::string_view a, std::string_view b);
};

class UnexpectedTokenException final : std::exception {
    const tokenizer::Token m_token;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit UnexpectedTokenException(const tokenizer::Token &token);
};
}
