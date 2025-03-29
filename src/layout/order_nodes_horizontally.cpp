#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/int_types.hpp"
#include "punkt/layout/common.hpp"

#include <vector>
#include <algorithm>
#include <string_view>

using namespace punkt;
using namespace punkt::layout;

static void barycenterSweepReorderOperator(Digraph &dg, const std::vector<float> &new_barycenters,
                                           const std::vector<float> &old_barycenters, const size_t rank,
                                           bool &out_improvement_found) {
    std::vector<size_t> rearrangement_order(new_barycenters.size());
    for (size_t i = 0; i < rearrangement_order.size(); i++) {
        rearrangement_order[i] = i;
    }
    std::ranges::sort(rearrangement_order, [&](const size_t a, const size_t b) {
        return new_barycenters.at(a) < new_barycenters.at(b);
    });
    bool rearrangement_order_has_effect = false;
    for (size_t i = 0; i < rearrangement_order.size(); i++) {
        if (rearrangement_order[i] != i) {
            rearrangement_order_has_effect = true;
            break;
        }
    }
    if (!rearrangement_order_has_effect) {
        return;
    }

    // rearrange orderings vector according to rearrangement order
    auto &ordering = dg.m_per_rank_orderings.at(rank);
    std::vector<std::string_view> rearranged_ordering;
    rearranged_ordering.reserve(ordering.size());
    for (const size_t idx: rearrangement_order) {
        rearranged_ordering.emplace_back(ordering.at(idx));
    }

    std::swap(ordering, rearranged_ordering);
    populateOrderingIndexAtRank(dg, rank);
    out_improvement_found = true;
}

static bool barycenterIteration(Digraph &dg, const bool is_downward_sweep, const float dampening) {
    bool improvement_found = false;
    float total_change = 0.0f;
    barycenterSweep(dg, is_downward_sweep, improvement_found, total_change, barycenterSweepReorderOperator,
                    BARYCENTER_USE_MEDIAN, dampening, false);
    const float average_change = total_change / static_cast<float>(dg.m_nodes.size());
    return improvement_found || average_change >= BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED *
           BARYCENTER_ORDERING_DAMPENING;
}

static void runBarycenter(Digraph &dg) {
    float dampening = BARYCENTER_ORDERING_DAMPENING;
    if (BARYCENTER_SWEEP_DIRECTION_IS_OUTER_LOOP) {
        const float orig_dampening = dampening;
        for (const bool is_downward_sweep: {false, true}) {
            dampening = orig_dampening;
            for (ssize_t barycenter_iter = 0; barycenter_iter < BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION ||
                                              BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION < 0; barycenter_iter++) {
                if (!barycenterIteration(dg, is_downward_sweep, dampening)) {
                    return;
                }
                dampening *= BARYCENTER_ORDERING_FADEOUT;
            }
        }
    } else {
        for (ssize_t barycenter_iter = 0; barycenter_iter < BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION ||
                                          BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION < 0; barycenter_iter++) {
            for (const bool is_downward_sweep: {false, true}) {
                if (!barycenterIteration(dg, is_downward_sweep, dampening)) {
                    return;
                }
            }
            dampening *= BARYCENTER_ORDERING_FADEOUT;
        }
    }
}

void Digraph::computeHorizontalOrderings() {
    clearGlobalState();

    // init with empty ordering vector for every rank
    m_per_rank_orderings.resize(m_rank_counts.size());
    m_per_rank_orderings_index.resize(m_rank_counts.size());
    populateInitialOrderings(*this);

    // populate barycenter x on each node
    for (size_t rank = 0; rank < m_rank_counts.size(); rank++) {
        for (size_t i = 0; i < m_rank_counts[rank]; i++) {
            const auto &node_name = m_per_rank_orderings.at(rank).at(i);
            auto &node = m_nodes.at(node_name);
            node.m_render_attrs.m_barycenter_x = static_cast<float>(i);
        }
    }
    runBarycenter(*this);

    if (BUBBLE_ORDERING_MAX_ITERS == 0) {
        return;
    }
    // reorder bubble-sort style until no adjacent node swap increases the score
    std::vector<float> rank_scores(m_per_rank_orderings.size());
    for (size_t rank = 0; rank < rank_scores.size(); rank++) {
        rank_scores[rank] = getRankOrderingScore(*this, rank);
    }
    for (ssize_t i = 0; i < BUBBLE_ORDERING_MAX_ITERS || BUBBLE_ORDERING_MAX_ITERS < 0; i++) {
        // iterate until no changes improve the score anymore (or until iteration limit is reached)
        bool improvement_found = false;
        // each iteration, we loop over every rank and every node in each rank in order and try to swap it with its
        // neighbour to the right. If that improves the score, we keep the change, otherwise, we discard it.
        for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
            rank_scores[i] = getRankOrderingScore(*this, rank);

            for (size_t node_idx = 0; node_idx < m_per_rank_orderings.at(rank).size() - 1; node_idx++) {
                // save the previous state so I can efficiently revert
                const size_t pre_swap_n_intersections_pr[2] = {g_n_intersections_pr[0], g_n_intersections_pr[1]};
                const size_t pre_swap_sum_dx_pr[2] = {g_sum_dx_pr[0], g_sum_dx_pr[1]};

                // attempt swapping node with neighbour
                if (const float new_rank_score = updateRankOrderingScoreAfterSwap(node_idx, node_idx + 1);
                    new_rank_score < rank_scores[rank]) {
                    rank_scores[rank] = new_rank_score;
                    swapNodesOnRank(m_per_rank_orderings[rank][node_idx], m_per_rank_orderings[rank][node_idx + 1]);
                    improvement_found = true;
                } else {
                    // equivalent to (but more efficient than) updateRankOrderingScoreAfterSwap(node_idx, node_idx + 1)
                    revertToPreSwapState(pre_swap_n_intersections_pr, pre_swap_sum_dx_pr, node_idx, node_idx + 1);
                }
            }
        }
        if (!improvement_found) {
            break;
        }
    }
}
