#include "punkt/dot.hpp"
#include "punkt/utils/utils.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <ranges>
#include <functional>
#include <unordered_set>
#include <cassert>
#include <numeric>

using namespace punkt;

static size_t size_max = std::numeric_limits<size_t>::max();
static size_t max_rank_range_start = size_max / 2;

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
    // Have to copy out names and sort alphabetically because iteration order on std::unordered_map is not guaranteed.
    // This may lead to different topologies using different STL implementations, which is undesirable.
    std::vector<std::string_view> node_names;
    node_names.reserve(dg.m_nodes.size());
    for (const Node &node: std::views::values(dg.m_nodes)) {
        node_names.emplace_back(node.m_name);
    }
    std::ranges::sort(node_names);

    std::vector<std::reference_wrapper<Node> > out;
    std::unordered_set<const Node *> visited = {};
    for (const std::string_view &name: node_names) {
        Node &node = dg.m_nodes.at(name);
        topoSortNodesImpl(dg, node, out, visited);
    }
    return out;
}

static size_t applyConstraints(size_t rank, Digraph &dg, const Node &node,
                               const std::unordered_set<std::string_view> &processed_nodes) {
    std::string_view constraints = getAttrOrDefault(node.m_attrs, "@constraints", "");
    while (!constraints.empty()) {
        const size_t constraint_end_idx = constraints.find(';');
        const std::string_view constraint_idx_str = constraints.substr(0, constraint_end_idx);
        const size_t constraint_idx = stringViewToSizeT(constraint_idx_str, "@constraints");

        const auto &[constraint_type, constrained_nodes] = dg.m_rank_constraints.at(constraint_idx);
        if (constraint_type == "min" || constraint_type == "source") {
            rank = 1;
        } else if (constraint_type == "same") {
            for (const auto &cnn: constrained_nodes) {
                if (processed_nodes.contains(cnn)) {
                    Node &other = dg.m_nodes.at(cnn);
                    rank = std::max(other.m_render_attrs.m_rank, rank);
                    other.m_render_attrs.m_rank = rank;
                }
            }
        } else if (constraint_type == "max" || constraint_type == "sink") {
            rank = size_max;
        }

        constraints = constraints.substr(constraint_end_idx, constraints.length() - constraint_end_idx);
        assert(constraints.starts_with(';'));
        constraints = constraints.substr(1, constraints.length() - 1);
    }
    return rank;
}

// max(parent.rank foreach parent) + 1
static size_t getInitialRank(Digraph &dg, const Node &node,
                             const std::unordered_set<std::string_view> &processed_nodes) {
    size_t max_rank = 0;
    for (const auto &ingoing_edge: node.m_ingoing) {
        const Node &parent = dg.m_nodes.at(ingoing_edge.get().m_source);
        max_rank = std::max(max_rank, parent.m_render_attrs.m_rank);
    }
    const size_t node_rank = max_rank >= max_rank_range_start ? max_rank - 1 : max_rank + 1;
    return applyConstraints(node_rank, dg, node, processed_nodes);
}

// Set rank to min(child.rank foreach child, node.rank) - 1.
// This is so that a parent-less node connected to another one won't have unnecessarily long connections and instead,
//  the parent is pulled towards the child because the child is constrained to where it is (at the numerically lowest
//  rank it is allowed to be), but the parent isn't yet at the numerically highest rank it is allowed to be towards its
//  children.
static void contractRank(const Digraph &dg, Node &node) {
    // Skip all nodes with constraints on them
    if (const std::string_view constraints = getAttrOrDefault(node.m_attrs, "@constraints", "");
        !constraints.empty()) {
        return;
    }

    // Set rank to min(child.rank foreach child, node.rank) - 1.
    size_t min_rank = max_rank_range_start;
    for (const auto &outgoing_edge: node.m_outgoing) {
        const Node &child = dg.m_nodes.at(outgoing_edge.m_dest);
        min_rank = std::min(min_rank, child.m_render_attrs.m_rank);
    }
    node.m_render_attrs.m_rank = min_rank >= max_rank_range_start ? node.m_render_attrs.m_rank : min_rank - 1;
}

static void normalizeRanks(Digraph &dg) {
    std::set<size_t> raw_ranks_set;
    for (const Node &node: std::views::values(dg.m_nodes)) {
        raw_ranks_set.insert(node.m_render_attrs.m_rank);
    }
    std::vector<size_t> raw_ranks{raw_ranks_set.begin(), raw_ranks_set.end()};
    std::ranges::sort(raw_ranks);

    std::map<size_t, size_t> mapping;
    for (size_t i = 0; i < raw_ranks.size(); ++i) {
        mapping[raw_ranks[i]] = i;
    }
    for (Node &node: std::views::values(dg.m_nodes)) {
        node.m_render_attrs.m_rank = mapping[node.m_render_attrs.m_rank];
    }
}

void Digraph::computeRanks() {
    std::unordered_set<std::string_view> processed_nodes;
    const std::vector<std::reference_wrapper<Node> > sorted_nodes = topoSortNodes(*this);
    for (auto node: std::ranges::reverse_view(sorted_nodes)) {
        node.get().m_render_attrs.m_rank = getInitialRank(*this, node, processed_nodes);
        processed_nodes.emplace(node.get().m_name);
    }
    for (auto node: sorted_nodes) {
        contractRank(*this, node);
    }
    normalizeRanks(*this);
    for (const Node &node: std::views::values(m_nodes)) {
        if (node.m_render_attrs.m_rank >= m_rank_counts.size()) {
            m_rank_counts.resize(node.m_render_attrs.m_rank + 1);
        }
        m_rank_counts.at(node.m_render_attrs.m_rank)++;
    }
}
