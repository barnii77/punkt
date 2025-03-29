#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/layout/common.hpp"

#include <vector>
#include <cassert>
#include <limits>
#include <cmath>

using namespace punkt;
using namespace punkt::layout;

static bool g_is_group_barycenter_pass = false;

constexpr float dist_required_to_touch = 2.0f;

// force the barycenter x back between it's left and right neighbours to maintain node order
static void legalizeBarycenters(Digraph &dg, const size_t rank) {
    const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
    const auto node_sep = static_cast<float>(dg.m_render_attrs.m_node_sep);
    for (const bool is_left_sweep: {false, true}) {
        // start the separation process below from the center node for better stability
        const auto start = static_cast<ssize_t>(rank_ordering.size() / 2);
        ssize_t stop, step;
        if (is_left_sweep) {
            stop = -1;
            step = -1;
        } else {
            stop = static_cast<ssize_t>(rank_ordering.size());
            step = 1;
        }

        // iteratively force nodes out of each others bounding boxes
        for (ssize_t i = start; i != stop; i += step) {
            if (i == start) {
                // the middle node (where we start from) should stay where it is
                continue;
            }
            Node &node = dg.m_nodes.at(rank_ordering.at(i));
            float prev_x_end = -std::numeric_limits<float>::infinity(), next_x_start = std::numeric_limits<
                float>::infinity();
            if (i > 0) {
                const Node &prev_node = dg.m_nodes.at(rank_ordering.at(i - 1));
                const auto w = static_cast<float>(prev_node.m_render_attrs.m_width);
                prev_x_end = prev_node.m_render_attrs.m_barycenter_x + w;
            }
            if (i < rank_ordering.size() - 1) {
                const Node &next_node = dg.m_nodes.at(rank_ordering.at(i + 1));
                const auto w = static_cast<float>(node.m_render_attrs.m_width);
                next_x_start = next_node.m_render_attrs.m_barycenter_x - w;
            }
            if (is_left_sweep) {
                node.m_render_attrs.m_barycenter_x = std::min(node.m_render_attrs.m_barycenter_x,
                                                              next_x_start - node_sep);
            } else {
                node.m_render_attrs.m_barycenter_x =
                        std::max(node.m_render_attrs.m_barycenter_x, prev_x_end + node_sep);
            }
        }
    }
}

static bool isTouchingPrev(const Digraph &dg, const std::vector<std::string_view> &rank_ordering, const size_t i,
                           const std::vector<float> &old_barycenters) {
    float prev_x_end = 0.0f;
    if (i > 0) {
        const Node &prev_node = dg.m_nodes.at(rank_ordering.at(i - 1));
        prev_x_end = old_barycenters[i - 1] + static_cast<float>(prev_node.m_render_attrs.m_width);
    }
    const float d = old_barycenters[i] - prev_x_end;
    return d - static_cast<float>(dg.m_render_attrs.m_node_sep) <= dist_required_to_touch;
}

static void barycenterXOptimizationOperator(Digraph &dg, const std::vector<float> &new_barycenters,
                                            const std::vector<float> &old_barycenters, const size_t rank,
                                            bool &out_improvement_found) {
    assert(new_barycenters.size() == old_barycenters.size());
    const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);

    float mean_bx = 0.0f;
    for (size_t i = 0; i < new_barycenters.size(); i++) {
        Node &node = dg.m_nodes.at(rank_ordering.at(i));
        node.m_render_attrs.m_barycenter_x -= static_cast<float>(node.m_render_attrs.m_width) / 2.0f;
        mean_bx += node.m_render_attrs.m_barycenter_x;
    }
    mean_bx /= static_cast<float>(new_barycenters.size());
    for (size_t i = 0; i < new_barycenters.size(); i++) {
        Node &node = dg.m_nodes.at(rank_ordering.at(i));
        node.m_render_attrs.m_barycenter_x = std::lerp(node.m_render_attrs.m_barycenter_x, mean_bx,
                                                       BARYCENTER_X_OPTIMIZATION_PULL_TOWARDS_MEAN) *
                                             BARYCENTER_X_OPTIMIZATION_REGULARIZATION;
    }

    if (g_is_group_barycenter_pass) {
        // average the change in barycenters across all nodes in a group and apply the average change to each of them.
        // A group of node refers to a set of adjacent nodes touching each other.
        std::vector<float> group_average_barycenter_change;
        std::vector<size_t> group_sizes;
        group_average_barycenter_change.reserve(new_barycenters.size());
        group_sizes.reserve(new_barycenters.size());
        float d_accum = 0.0f;
        size_t group_size = 0;
        for (size_t i = 0; i < new_barycenters.size(); i++, group_size++) {
            if (i > 0 && !isTouchingPrev(dg, rank_ordering, i, old_barycenters)) {
                group_average_barycenter_change.emplace_back(d_accum / static_cast<float>(group_size));
                group_sizes.emplace_back(group_size);
                d_accum = 0.0f;
                group_size = 0;
            }
            const float d = new_barycenters[i] - old_barycenters[i];
            d_accum += d;
        }
        if (group_size > 0) {
            group_average_barycenter_change.emplace_back(d_accum / static_cast<float>(group_size));
            group_sizes.emplace_back(group_size);
        }

        // assign the group averaged barycenters
        for (size_t i = 0, group_idx = 0, group_inner_idx = 0; i < new_barycenters.size(); i++) {
            assert(group_sizes.at(group_idx) > 0);
            if (group_inner_idx >= group_sizes.at(group_idx)) {
                group_inner_idx = 0;
                group_idx++;
            }
            Node &node = dg.m_nodes.at(rank_ordering.at(i));
            node.m_render_attrs.m_barycenter_x = old_barycenters[i] + group_average_barycenter_change.at(group_idx);
        }
    }

    legalizeBarycenters(dg, rank);

    if (!out_improvement_found) {
        float total_change = 0.0f;
        for (size_t i = 0; i < new_barycenters.size(); i++) {
            const Node &node = dg.m_nodes.at(rank_ordering.at(i));
            total_change += node.m_render_attrs.m_barycenter_x - old_barycenters[i];
        }
        if (const float average_change = total_change / static_cast<float>(new_barycenters.size());
            average_change >= BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED *
            BARYCENTER_X_OPTIMIZATION_DAMPENING) {
            out_improvement_found = true;
        }
    }
}

static void recomputeGraphDimensions(Digraph &dg) {
    size_t x_min = dg.m_render_attrs.m_graph_x, x_max = dg.m_render_attrs.m_graph_x + dg.m_render_attrs.m_graph_width;
    for (size_t rank = 0; rank < dg.m_per_rank_orderings.size(); rank++) {
        for (const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
             const auto &name: rank_ordering) {
            const Node &node = dg.m_nodes.at(name);
            x_min = std::min(x_min, node.m_render_attrs.m_x);
            x_max = std::max(x_max, node.m_render_attrs.m_x + node.m_render_attrs.m_width);
        }
    }
    assert(x_min == 0);

    // TODO this has to also recompute the graph bounding box and margin stuff... maybe I should only do this here
    // in the first place and just move the logic over here?
    // compute graph dimensions
    const size_t inner_width = std::ranges::max_element(
        dg.m_render_attrs.m_rank_render_attrs,
        [](const RankRenderAttrs &rra1, const RankRenderAttrs &rra2) {
            return rra1.m_rank_width < rra2.m_rank_width;
        })->m_rank_width;
    const float padding_fract = static_cast<float>(dg.m_render_attrs.m_graph_width - inner_width) / 2.0f;
    const auto left_padding = static_cast<size_t>(std::floorf(padding_fract));
    const auto right_padding = static_cast<size_t>(std::ceilf(padding_fract));
    dg.m_render_attrs.m_graph_width = x_max - x_min + left_padding + right_padding;

    // position graph label
    if (!dg.m_render_attrs.m_label_quads.empty()) {
        const size_t label_old_x = dg.m_render_attrs.m_label_quads.front().m_left;
        const size_t label_width = dg.m_render_attrs.m_label_quads.back().m_right - label_old_x;
        const size_t label_new_x = dg.m_render_attrs.m_graph_x + (dg.m_render_attrs.m_graph_width - label_width) / 2;
        const ssize_t x_adjustment = static_cast<ssize_t>(label_new_x) - static_cast<ssize_t>(label_old_x);
        for (GlyphQuad &gq: dg.m_render_attrs.m_label_quads) {
            gq.m_left += x_adjustment;
            gq.m_right += x_adjustment;
        }
    }

    // apply padding to each node again
    for (size_t rank = 0; rank < dg.m_per_rank_orderings.size(); rank++) {
        for (const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
             const auto &name: rank_ordering) {
            Node &node = dg.m_nodes.at(name);
            node.m_render_attrs.m_x += left_padding;
        }
    }
}

static bool barycenterIteration(Digraph &dg, const bool is_downward_sweep, const float dampening) {
    bool improvement_found = false;
    float total_change = 0.0f;
    barycenterSweep(dg, is_downward_sweep, improvement_found, total_change, barycenterXOptimizationOperator,
                    BARYCENTER_X_OPTIMIZATION_USE_MEDIAN, dampening, true);
    const float average_change = total_change / static_cast<float>(dg.m_nodes.size());
    return improvement_found || average_change >= BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED *
           BARYCENTER_X_OPTIMIZATION_DAMPENING;
}

static void runBarycenter(Digraph &dg) {
    float dampening = BARYCENTER_X_OPTIMIZATION_DAMPENING;
    if (BARYCENTER_X_OPTIMIZATION_SWEEP_DIRECTION_IS_OUTER_LOOP) {
        const float orig_dampening = dampening;
        for (const bool is_downward_sweep: {false, true}) {
            dampening = orig_dampening;
            for (ssize_t barycenter_iter = 0; barycenter_iter < BARYCENTER_X_OPTIMIZATION_MAX_ITERS ||
                                              BARYCENTER_X_OPTIMIZATION_MAX_ITERS < 0; barycenter_iter++) {
                for (const bool is_group_pass: {false, true}) {
                    g_is_group_barycenter_pass = is_group_pass;
                    if (!barycenterIteration(dg, is_downward_sweep, dampening)) {
                        return;
                    }
                }
                dampening *= BARYCENTER_X_OPTIMIZATION_FADEOUT;
            }
        }
    } else {
        for (ssize_t barycenter_iter = 0; barycenter_iter < BARYCENTER_X_OPTIMIZATION_MAX_ITERS ||
                                          BARYCENTER_X_OPTIMIZATION_MAX_ITERS < 0; barycenter_iter++) {
            for (const bool is_group_pass: {false, true}) {
                g_is_group_barycenter_pass = is_group_pass;
                for (const bool is_downward_sweep: {false, true}) {
                    if (!barycenterIteration(dg, is_downward_sweep, dampening)) {
                        return;
                    }
                }
            }
            dampening *= BARYCENTER_X_OPTIMIZATION_FADEOUT;
        }
    }
}

void Digraph::optimizeGraphLayout() {
    if (const std::string_view &x_opt = getAttrOrDefault(m_attrs, "punktxopt", "true");
        !caseInsensitiveEquals(x_opt, "true")) {
        return;
    }
    clearGlobalState();

    // populate barycenter x on each node
    for (size_t rank = 0; rank < m_rank_counts.size(); rank++) {
        for (size_t i = 0; i < m_rank_counts[rank]; i++) {
            const auto &node_name = m_per_rank_orderings.at(rank).at(i);
            auto &node = m_nodes.at(node_name);
            node.m_render_attrs.m_barycenter_x =
                    static_cast<float>(node.m_render_attrs.m_x) - static_cast<float>(node.m_render_attrs.m_width) /
                    2.0f;
        }
    }

    runBarycenter(*this);

    // find the minimum barycenter x and adjust so that minimum becomes 0
    ssize_t x_min = std::numeric_limits<ssize_t>::max();
    for (size_t rank = 0; rank < m_rank_counts.size(); rank++) {
        const auto &rank_ordering = m_per_rank_orderings.at(rank);
        for (size_t i = 0; i < m_rank_counts[rank]; i++) {
            const auto &node_name = rank_ordering.at(i);
            const auto &node = m_nodes.at(node_name);
            x_min = std::min(x_min, static_cast<ssize_t>(node.m_render_attrs.m_barycenter_x));
        }
    }
    // apply the final barycenter positions as the new x positions
    for (size_t rank = 0; rank < m_rank_counts.size(); rank++) {
        for (size_t i = 0; i < m_rank_counts[rank]; i++) {
            const auto &node_name = m_per_rank_orderings.at(rank).at(i);
            auto &node = m_nodes.at(node_name);
            node.m_render_attrs.m_x = static_cast<size_t>(
                static_cast<ssize_t>(node.m_render_attrs.m_barycenter_x) - x_min);
        }
    }

    recomputeGraphDimensions(*this);
}
