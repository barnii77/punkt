#include "punkt/dot.hpp"
#include "punkt/int_types.hpp"

#include <vector>
#include <type_traits>
#include <algorithm>

using namespace punkt;

static const Node &getOtherNode(const Digraph &dg, const Edge &e, const std::string_view &name) {
    const Node &src = dg.m_nodes.at(e.m_source);
    if (src.m_name == name) {
        return dg.m_nodes.at(e.m_dest);
    }
    return src;
}

static void emplaceEdgeWithRankDiff(const Digraph &dg, Edge &edge, const Node &node, const int expected_rank_diff,
                                    std::vector<std::reference_wrapper<Edge> > &edges) {
    if (const Node &other_node = getOtherNode(dg, edge, node.m_name);
        static_cast<ssize_t>(other_node.m_render_attrs.m_rank) - static_cast<ssize_t>(node.m_render_attrs.m_rank) ==
        expected_rank_diff) {
        edges.emplace_back(edge);
    }
}

// TODO implement for different shapes
static size_t getNodeTopHeightAt(const Node &node, const float x) {
    return node.m_render_attrs.m_y;
}

// TODO implement for different shapes
static size_t getNodeBottomHeightAt(const Node &node, const float x) {
    return node.m_render_attrs.m_y + node.m_render_attrs.m_height;
}

void Digraph::computeEdgeLayout() {
    std::vector<std::reference_wrapper<Edge> > edges;
    for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
        const RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(rank);

        for (const std::string_view &name: m_per_rank_orderings.at(rank)) {
            Node &node = m_nodes.at(name);

            for (const bool is_upward_edge_pass: {false, true}) {
                edges.clear();
                edges.reserve(node.m_ingoing.size() + node.m_outgoing.size());
                const int expected_rank_diff = is_upward_edge_pass ? -1 : 1;
                for (Edge &edge: node.m_outgoing) {
                    emplaceEdgeWithRankDiff(*this, edge, node, expected_rank_diff, edges);
                }
                for (auto edge: node.m_ingoing) {
                    emplaceEdgeWithRankDiff(*this, edge, node, expected_rank_diff, edges);
                }

                // sort edges by the order of the other node (they're all in the same rank). I need to do this because
                // I space out the edges across this node's surface.
                std::ranges::sort(edges, [&](const Edge &a, const Edge &b) {
                    const Node &other_node_a = getOtherNode(*this, a, node.m_name);
                    const Node &other_node_b = getOtherNode(*this, b, node.m_name);
                    const size_t ordering_idx_a = m_per_rank_orderings_index.at(other_node_a.m_render_attrs.m_rank).at(
                        other_node_a.m_name);
                    const size_t ordering_idx_b = m_per_rank_orderings_index.at(other_node_b.m_render_attrs.m_rank).at(
                        other_node_b.m_name);
                    return ordering_idx_a < ordering_idx_b;
                });

                // dx is the spacing between edges. They are spaced such that there is even distance between all ingoing
                // and outgoing edges and the bounding box of the node, i.e. for node width 5 with 2 edges:
                // .|.|.
                // #####
                const float dx = static_cast<float>(node.m_render_attrs.m_width) / static_cast<float>(edges.size() + 1);
                float x = dx;
                for (auto e: edges) {
                    Edge &edge = e;
                    const auto x_pixel = node.m_render_attrs.m_x + static_cast<size_t>(x);

                    // depending on whether we are doing upward pass (i.e. handling upward or downward edges), we need
                    // to emplace in a different order and obviously compute the points differently. This different
                    // insertion order is required because the trajectory is a sequence of points to be connected.
                    if (is_upward_edge_pass) {
                        Vector2 on_surface(x_pixel, getNodeTopHeightAt(node, x));
                        Vector2 on_rank_border(x_pixel, rra.m_rank_y);
                        edge.m_render_attrs.m_trajectory.emplace_back(on_rank_border);
                        edge.m_render_attrs.m_trajectory.emplace_back(on_surface);
                    } else {
                        Vector2 on_surface(x_pixel, getNodeBottomHeightAt(node, x));
                        Vector2 on_rank_border(x_pixel, rra.m_rank_y + rra.m_rank_height);
                        edge.m_render_attrs.m_trajectory.emplace_back(on_surface);
                        edge.m_render_attrs.m_trajectory.emplace_back(on_rank_border);
                    }

                    x += dx;
                }
            }
        }
    }
}
