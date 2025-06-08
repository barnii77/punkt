#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/layout/common.hpp"

#include <vector>
#include <ranges>
#include <numeric>
#include <cassert>
#include <limits>
#include <cmath>
#include <map>

using namespace punkt;
using namespace punkt::layout;

static bool g_is_group_barycenter_sweep = false;
static bool g_is_downward_barycenter_sweep = false;
static const XOptPipelineStageSettings *g_pss = nullptr;
static std::map<size_t, std::vector<float> > g_old_per_rank_barycenters{};

constexpr float dist_required_to_touch = 5.0f;

static void forceApartMiddleNodes(Digraph &dg, const float node_sep,
                                  const std::vector<std::string_view> &rank_ordering) {
    if (rank_ordering.size() % 2 == 1) {
        return;
    }
    // only have to force them apart if there are 2 middle nodes
    assert(!rank_ordering.empty());
    const size_t middle_idx = rank_ordering.size() / 2 - 1;
    auto &a = dg.m_nodes.at(rank_ordering.at(middle_idx));
    auto &b = dg.m_nodes.at(rank_ordering.at(middle_idx + 1));
    const auto a_w = static_cast<float>(a.m_render_attrs.m_width);
    const float required_shift = (a.m_render_attrs.m_barycenter_x + a_w - b.m_render_attrs.m_barycenter_x + node_sep) /
                                 2.0f;
    if (required_shift <= 0.0f) {
        return;
    }
    a.m_render_attrs.m_barycenter_x -= required_shift;
    b.m_render_attrs.m_barycenter_x += required_shift;
}

// TODO remove
static void printNodeBarycenters(const Digraph &dg, const std::vector<std::string_view> &rank_ordering,
                                 const bool is_pre) {
    // TODO re-enable printing
    return;
    for (const auto &n: rank_ordering) {
        const Node &node = dg.m_nodes.at(n);
        std::cout << (is_pre ? "(Pre) " : "(Post) ") << node.m_name << ": " << node.m_render_attrs.m_barycenter_x <<
                std::endl;
    }
}

static float meanBarycenterOnRank(const Digraph &dg, const std::vector<std::string_view> &rank_ordering) {
    float out = 0.0f;
    for (const auto &name: rank_ordering) {
        const Node &node = dg.m_nodes.at(name);
        out += node.m_render_attrs.m_barycenter_x;
    }
    return rank_ordering.empty() ? 0.0f : out / static_cast<float>(rank_ordering.size());
}

static bool isTouchingPrev(const Digraph &dg, const std::vector<std::string_view> &rank_ordering, const size_t i,
                           const std::vector<float> &old_barycenters) {
    float prev_x_end = 0.0f;
    if (i > 0) {
        const Node &prev_node = dg.m_nodes.at(rank_ordering.at(i - 1));
        prev_x_end = old_barycenters[i - 1] + static_cast<float>(prev_node.m_render_attrs.m_width);
    }
    const Node &node = dg.m_nodes.at(rank_ordering.at(i));
    const float current_x_start = old_barycenters[i] - static_cast<float>(node.m_render_attrs.m_width) / 2.0f;
    const float d = current_x_start - prev_x_end;
    return d - static_cast<float>(dg.m_render_attrs.m_node_sep) <= dist_required_to_touch;
}

// force the barycenter x back between it's left and right neighbours to maintain node order
static void legalizeBarycenters(Digraph &dg, const size_t rank, const XOptPipelineStageSettings &pss) {
    if (BARYCENTER_X_OPTIMIZATION_REORDER_BY_BARYCENTER_X_BEFORE_LEGALIZE) {
        bool trash;
        reorderRankByBarycenterX(dg, rank, trash);
    }

    const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
    const auto node_sep = static_cast<float>(dg.m_render_attrs.m_node_sep);
    float mean_before_legalize = 0.0f;
    if (pss.m_legalizer_settings.m_try_cancel_mean_shift) {
        mean_before_legalize = meanBarycenterOnRank(dg, rank_ordering);
    }

    printNodeBarycenters(dg, rank_ordering, true);
    forceApartMiddleNodes(dg, node_sep, rank_ordering);

    const std::vector old_barycenters_cpy(std::move(g_old_per_rank_barycenters.at(rank)));
    g_old_per_rank_barycenters[rank] = std::vector<float>(old_barycenters_cpy.size());
    auto &old_barycenters_glob = g_old_per_rank_barycenters[rank];

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
            float prev_x_end = -std::numeric_limits<float>::infinity(), max_current_x_start = std::numeric_limits<
                float>::infinity();
            const auto w_self = static_cast<float>(node.m_render_attrs.m_width);
            if (i > 0) {
                const Node &prev_node = dg.m_nodes.at(rank_ordering.at(i - 1));
                const auto w = static_cast<float>(prev_node.m_render_attrs.m_width);
                prev_x_end = prev_node.m_render_attrs.m_barycenter_x + (w + w_self) / 2.0f;
            }
            if (i < rank_ordering.size() - 1) {
                const Node &next_node = dg.m_nodes.at(rank_ordering.at(i + 1));
                const auto w = static_cast<float>(next_node.m_render_attrs.m_width);
                max_current_x_start = next_node.m_render_attrs.m_barycenter_x - (w + w_self) / 2.0f;
            }

            const bool explode_node_sep = pss.m_legalizer_settings.m_legalizer_special_instruction.m_type ==
                                          XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction::Type::explode_inter_group_node_sep;
            const float node_sep_explode_factor = explode_node_sep
                                                      ? pss.m_legalizer_settings.m_legalizer_special_instruction.
                                                      m_explode_inter_group_node_sep_factor
                                                      : 1.0f;
            float exploded_node_sep = node_sep;
            if (node_sep_explode_factor != 1.0f && !isTouchingPrev(dg, rank_ordering, is_left_sweep ? i + 1 : i,
                                                                   old_barycenters_cpy)) {
                exploded_node_sep *= node_sep_explode_factor;
            }

            float new_barycenter_x;
            if (is_left_sweep) {
                new_barycenter_x = std::min(node.m_render_attrs.m_barycenter_x,
                                            max_current_x_start - exploded_node_sep);
            } else {
                new_barycenter_x = std::max(node.m_render_attrs.m_barycenter_x, prev_x_end + exploded_node_sep);
            }
            old_barycenters_glob.at(i) = node.m_render_attrs.m_barycenter_x;
            node.m_render_attrs.m_barycenter_x = new_barycenter_x;
        }
    }
    printNodeBarycenters(dg, rank_ordering, false);

    if (pss.m_legalizer_settings.m_try_cancel_mean_shift) {
        const float mean_after_legalize = meanBarycenterOnRank(dg, rank_ordering);
        const float cancel_motion = (mean_before_legalize - mean_after_legalize) / static_cast<float>(
                                        rank_ordering.size());
        for (const auto &name: rank_ordering) {
            Node &node = dg.m_nodes.at(name);
            node.m_render_attrs.m_barycenter_x += cancel_motion;
        }
    }
}

static std::vector<size_t> findGroups(const Digraph &dg, const std::vector<std::string_view> &rank_ordering,
                                      const std::vector<float> &barycenters) {
    std::vector<size_t> group_sizes;
    group_sizes.reserve(barycenters.size());
    size_t group_size = 0;
    for (size_t i = 0; i < barycenters.size(); i++, group_size++) {
        if (i > 0 && !isTouchingPrev(dg, rank_ordering, i, barycenters)) {
            group_sizes.emplace_back(group_size);
            group_size = 0;
        }
    }
    if (group_size > 0) {
        group_sizes.emplace_back(group_size);
    }
    return group_sizes;
}

/// this function has to match the signature given by @code BarycenterSweepOperatorFunc @endcode
static void barycenterXOptimizationOperator(Digraph &dg, std::vector<float> new_barycenters,
                                            std::vector<float> old_barycenters, const size_t rank,
                                            bool &out_improvement_found) {
    assert(new_barycenters.size() == old_barycenters.size());
    float regularization_strength = 0.0f, pull_towards_mean_strength = 0.0f;
    if (g_pss) {
        regularization_strength = g_pss->m_regularization;
        pull_towards_mean_strength = g_pss->m_pull_towards_mean;
    }
    const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);

    const float mean_bx = std::accumulate(new_barycenters.begin(), new_barycenters.end(), 0.0f) /
                          static_cast<float>(new_barycenters.size());
    for (const auto &node_name: rank_ordering) {
        Node &node = dg.m_nodes.at(node_name);
        const float regularization = (rank == BARYCENTER_X_OPTIMIZATION_REGULARIZATION_RANK ||
                                      BARYCENTER_X_OPTIMIZATION_REGULARIZATION_RANK == -1) && (
                                         !BARYCENTER_X_OPTIMIZATION_REGULARIZATION_ONLY_ON_DOWNWARD ||
                                         g_is_downward_barycenter_sweep)
                                         ? regularization_strength
                                         : 1.0f;
        node.m_render_attrs.m_barycenter_x = std::lerp(node.m_render_attrs.m_barycenter_x, mean_bx,
                                                       pull_towards_mean_strength) * regularization;
    }

    if (g_is_group_barycenter_sweep) {
        // average the change in barycenters across all nodes in a group and apply the average change to each of them.
        // A group of node refers to a set of adjacent nodes touching each other.
        const auto group_sizes = findGroups(dg, rank_ordering, old_barycenters);
        std::vector<float> group_average_barycenter_change;
        group_average_barycenter_change.reserve(group_sizes.size());
        size_t idx = 0;
        for (const auto group_size: group_sizes) {
            float d_accum = 0.0f;
            float weight_accum = 0.0f;
            for (size_t i = idx; i < idx + group_size; i++) {
                const float d = new_barycenters[i] - old_barycenters[i];
                const float weight = dg.m_nodes.at(rank_ordering.at(i)).m_render_attrs.m_is_ghost
                                         ? BARYCENTER_X_OPTIMIZATION_GHOST_NODE_RELATIVE_WEIGHT
                                         : 1.0f;
                d_accum += d * weight;
                weight_accum += weight;
            }
            group_average_barycenter_change.emplace_back(d_accum / weight_accum);
            idx += group_size;
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

    if (!out_improvement_found) {
        float total_change = 0.0f;
        for (size_t i = 0; i < new_barycenters.size(); i++) {
            const Node &node = dg.m_nodes.at(rank_ordering.at(i));
            total_change += node.m_render_attrs.m_barycenter_x - old_barycenters[i];
        }
        if (const float average_change = total_change / static_cast<float>(new_barycenters.size());
            average_change >= BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED) {
            out_improvement_found = true;
        }
    }

    g_old_per_rank_barycenters.insert_or_assign(rank, std::move(old_barycenters));

    if (g_pss && g_pss->m_legalizer_settings.m_legalization_timing ==
        XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::in_barycenter_operator) {
        legalizeBarycenters(dg, rank, *g_pss);
    }
}

static void recomputeGraphDimensions(Digraph &dg) {
    size_t x_min = std::numeric_limits<size_t>::max(), x_max = 0;
    for (size_t rank = 0; rank < dg.m_per_rank_orderings.size(); rank++) {
        for (const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
             const auto &name: rank_ordering) {
            const Node &node = dg.m_nodes.at(name);
            x_min = std::min(x_min, node.m_render_attrs.m_x);
            x_max = std::max(x_max, node.m_render_attrs.m_x + node.m_render_attrs.m_width);
        }
    }
    assert(x_min == 0);

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

// legalization pass to make sure the determined node order is respected
static void runLegalizationPass(Digraph &dg, const XOptPipelineStageSettings &pss,
                                const bool ignore_missing_ranks = false) {
    for (size_t rank = 0; rank < dg.m_per_rank_orderings.size(); rank++) {
        if (ignore_missing_ranks && !g_old_per_rank_barycenters.contains(rank)) {
            continue;
        }
        legalizeBarycenters(dg, rank, pss);
    }
}

static bool barycenterIteration(Digraph &dg, const bool is_downward_sweep, const float dampening,
                                const XOptPipelineStageSettings &pss, const ssize_t start_rank = -1) {
    bool improvement_found = false;
    float total_change = 0.0f;
    barycenterSweep(dg, is_downward_sweep, improvement_found, total_change, barycenterXOptimizationOperator,
                    BARYCENTER_X_OPTIMIZATION_USE_MEDIAN, dampening, start_rank, start_rank == -1 ? -1 : 1);
    if (g_pss && g_pss->m_legalizer_settings.m_legalization_timing ==
        XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::after_iteration) {
        runLegalizationPass(dg, pss, true);
    }
    const float average_change = total_change / static_cast<float>(dg.m_nodes.size());
    return improvement_found || average_change >= BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED * dampening;
}

static void runBarycenterPipelineStage(Digraph &dg, const XOptPipelineStageSettings &pss) {
    g_pss = &pss;
    float dampening = pss.m_initial_dampening;
    if (pss.m_sweep_mode == SweepMode::sweep_direction_is_outer_loop) {
        for (const auto &sweep_settings: pss.m_sweep_settings) {
            g_is_downward_barycenter_sweep = sweep_settings.m_is_downward_sweep;
            g_is_group_barycenter_sweep = sweep_settings.m_is_group_sweep;
            dampening = pss.m_initial_dampening;
            for (ssize_t barycenter_iter = 0; barycenter_iter < pss.m_max_iters || pss.m_max_iters < 0;
                 barycenter_iter++) {
                if (!barycenterIteration(dg, sweep_settings.m_is_downward_sweep, dampening, pss)) {
                    return;
                }
                dampening *= pss.m_dampening_fadeout;
            }
        }
    } else if (pss.m_sweep_mode == SweepMode::sweep_direction_is_innermost_loop) {
        for (ssize_t barycenter_iter = 0; barycenter_iter < pss.m_max_iters || pss.m_max_iters < 0; barycenter_iter
             ++) {
            // In this branch, I swap the downward and upward sweep modes per rank before moving onto the next rank.
            // What normally happens is that it does a full upward sweep for all ranks, then a full downward sweep
            // across all ranks, etc.
            const auto i_limit = static_cast<ssize_t>(dg.m_per_rank_orderings.size() - 1);
            for (ssize_t i = 0; i < i_limit; i++) {
                for (const auto &sweep_settings: pss.m_sweep_settings) {
                    if (sweep_settings.m_sweep_n_ranks_limit >= 0 &&
                        i_limit - 1 - i >= sweep_settings.m_sweep_n_ranks_limit) {
                        continue;
                    }
                    g_is_downward_barycenter_sweep = sweep_settings.m_is_downward_sweep;
                    g_is_group_barycenter_sweep = sweep_settings.m_is_group_sweep;
                    if (!barycenterIteration(dg, sweep_settings.m_is_downward_sweep, dampening, pss, i)) {
                        return;
                    }
                }
            }
            dampening *= pss.m_dampening_fadeout;
        }
    } else {
        assert(pss.m_sweep_mode == SweepMode::normal);
        for (ssize_t barycenter_iter = 0; barycenter_iter < pss.m_max_iters || pss.m_max_iters < 0; barycenter_iter++) {
            for (const auto &sweep_settings: pss.m_sweep_settings) {
                g_is_downward_barycenter_sweep = sweep_settings.m_is_downward_sweep;
                g_is_group_barycenter_sweep = sweep_settings.m_is_group_sweep;
                if (!barycenterIteration(dg, sweep_settings.m_is_downward_sweep, dampening, pss)) {
                    return;
                }
                dampening *= pss.m_dampening_fadeout;
            }
        }
    }
}

static void runBarycenter(Digraph &dg) {
    for (const XOptPipelineStageSettings &pss: BARYCENTER_X_OPTIMIZATION_PIPELINE) {
        for (size_t i = 0; i < pss.m_repeats; i++) {
            runBarycenterPipelineStage(dg, pss);
            if (pss.m_legalizer_settings.m_legalization_timing ==
                XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::after_pipeline_stage) {
                runLegalizationPass(dg, pss);
            }
        }
    }
}

void Digraph::optimizeGraphLayout() {
    constexpr std::string_view punkt_x_opt_attr_name = "punktxopt";
    if (const std::string_view &x_opt = getAttrOrDefault(m_attrs, punkt_x_opt_attr_name, "true");
        !caseInsensitiveEquals(x_opt, "true")) {
        if (!caseInsensitiveEquals(x_opt, "false")) {
            throw IllegalAttributeException(std::string(punkt_x_opt_attr_name), std::string(x_opt));
        }
        return;
    }
    clearGlobalState();

    // populate barycenter x on each node
    for (size_t rank = 0; rank < m_rank_counts.size(); rank++) {
        for (size_t i = 0; i < m_rank_counts[rank]; i++) {
            const auto &node_name = m_per_rank_orderings.at(rank).at(i);
            auto &node = m_nodes.at(node_name);
            node.m_render_attrs.m_barycenter_x = static_cast<float>(node.m_render_attrs.m_x) +
                                                 static_cast<float>(node.m_render_attrs.m_width) / 2.0f;
        }
    }

    runBarycenter(*this);

    // convert m_barycenter_x from center to left edge position
    for (Node &node: std::views::values(m_nodes)) {
        node.m_render_attrs.m_barycenter_x -= static_cast<float>(node.m_render_attrs.m_width) / 2.0f;
    }

    // find the minimum barycenter x and adjust so that minimum becomes 0
    ssize_t x_min = std::numeric_limits<ssize_t>::max();
    for (auto &rank_ordering: m_per_rank_orderings) {
        for (const auto &node_name: rank_ordering) {
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
