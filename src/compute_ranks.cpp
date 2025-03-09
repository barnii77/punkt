#include "punkt/dot.hpp"

#include <algorithm>
#include <ranges>
#include <functional>
#include <unordered_set>
#include <cassert>

using namespace dot;

static void topoSortNodesImpl(Digraph &dg, Node &node, std::vector<std::reference_wrapper<Node> > &out,
                              std::unordered_set<const Node *> &visited) {
    if (visited.contains(&node)) {
        return;
    }
    visited.emplace(&node);
    for (const Edge &outgoing_edge: node.m_outgoing) {
        Node &child = dg.m_nodes.at(outgoing_edge.m_dest);
        topoSortNodesImpl(dg, child, out, visited);
    }
    out.emplace_back(node);
}

static std::vector<std::reference_wrapper<Node> > topoSortNodes(Digraph &dg) {
    std::vector<std::reference_wrapper<Node> > out;
    std::unordered_set<const Node *> visited = {};
    for (Node &node: std::views::values(dg.m_nodes)) {
        topoSortNodesImpl(dg, node, out, visited);
    }
    return out;
}

// max(parent.rank foreach parent) + 1
static size_t getRank(const Digraph &dg, const Node &node) {
    size_t max_rank = 0;
    for (const auto &ingoing_edge: node.m_ingoing) {
        const Node &parent = dg.m_nodes.at(ingoing_edge.get().m_source);
        max_rank = std::max(max_rank, parent.m_render_attrs.m_rank);
    }
    return max_rank + 1;
}

void Digraph::computeRanks() {
    for (const std::vector<std::reference_wrapper<Node> > sorted_nodes = topoSortNodes(*this);
         auto node: std::ranges::reverse_view(sorted_nodes)) {
        node.get().m_render_attrs.m_rank = getRank(*this, node);
    }
    for (Node &node: std::views::values(m_nodes)) {
        // ranks are currently 1 to n + 1, but we want them 0 to n
        assert(node.m_render_attrs.m_rank > 0);
        const size_t rank = --node.m_render_attrs.m_rank;
        if (rank >= m_rank_counts.size()) {
            m_rank_counts.resize(rank + 1);
        }
        m_rank_counts.at(rank)++;
    }
}
