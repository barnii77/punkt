#include "punkt/dot.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/int_types.hpp"

#include <vector>
#include <type_traits>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

using namespace punkt;

static const Node &getOtherNode(const Digraph &dg, const Edge &e, const std::string_view &name) {
    const Node &src = dg.m_nodes.at(e.m_source);
    if (src.m_name == name) {
        return dg.m_nodes.at(e.m_dest);
    }
    return src;
}

static size_t sumOfX(const size_t accum, const Vector2<size_t> &v) {
    return accum + v.x;
}

static void emplaceEdgeWithRankDiff(const Digraph &dg, Edge &edge, const Node &node, const int expected_rank_diff,
                                    std::vector<std::reference_wrapper<Edge> > &edges) {
    if (const Node &other_node = getOtherNode(dg, edge, node.m_name);
        static_cast<ssize_t>(other_node.m_render_attrs.m_rank) - static_cast<ssize_t>(node.m_render_attrs.m_rank) ==
        expected_rank_diff) {
        edges.emplace_back(edge);
    }
}

static float getEllipseHeightAt(const Node &node, float x) {
    // `a` is horizontal radius, `b` is vertical radius, `x` is x position in a centered cartesian coordinate system
    const auto a = static_cast<float>(node.m_render_attrs.m_width) / 2.0f;
    const auto b = static_cast<float>(node.m_render_attrs.m_height) / 2.0f;
    x -= a; // move x E [0, 2a] to [-a, a]
    return b * std::sqrt(1.0f - x * x / (a * a));
}

// TODO implement for different shapes
static size_t getNodeTopHeightAt(const Node &node, const float x) {
    if (const std::string_view shape = getAttrOrDefault(node.m_attrs, "shape", default_shape);
        shape == "ellipse") {
        return static_cast<size_t>(std::round(
            static_cast<float>(node.m_render_attrs.m_y) + static_cast<float>(node.m_render_attrs.m_height) / 2.0f -
            getEllipseHeightAt(node, x)));
    }
    return node.m_render_attrs.m_y;
}

// TODO implement for different shapes
static size_t getNodeBottomHeightAt(const Node &node, const float x) {
    if (const std::string_view shape = getAttrOrDefault(node.m_attrs, "shape", default_shape);
        shape == "ellipse") {
        return static_cast<size_t>(std::round(
            static_cast<float>(node.m_render_attrs.m_y) + static_cast<float>(node.m_render_attrs.m_height) / 2.0f +
            getEllipseHeightAt(node, x)));
    }
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
                    if (ordering_idx_a < ordering_idx_b) {
                        return true;
                    } else if (ordering_idx_a == ordering_idx_b) {
                        if (!a.m_render_attrs.m_trajectory.empty()) {
                            assert(a.m_render_attrs.m_trajectory.size() == b.m_render_attrs.m_trajectory.size());
                            const size_t a_total_x = std::accumulate(a.m_render_attrs.m_trajectory.begin(),
                                                                     a.m_render_attrs.m_trajectory.end(), 0ull, sumOfX);
                            const size_t b_total_x = std::accumulate(b.m_render_attrs.m_trajectory.begin(),
                                                                     b.m_render_attrs.m_trajectory.end(), 0ull, sumOfX);
                            return a_total_x <= b_total_x;
                        }
                        return true;
                    } else {
                        return false;
                    }
                });

                // dx is the spacing between edges. They are spaced such that there is even distance between all ingoing
                // and outgoing edges and the bounding box of the node, i.e. for node width 5 with 2 edges:
                // .|.|.
                // #####
                const float dx = static_cast<float>(node.m_render_attrs.m_width) / static_cast<float>(edges.size() + 1);
                float x = dx;
                for (auto e: edges) {
                    Edge &edge = e;
                    edge.m_render_attrs.m_trajectory.reserve(expected_edge_line_length);
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
