#pragma once

#include "punkt/dot.hpp"
#include "punkt/int_types.hpp"

#include <vector>
#include <functional>

namespace punkt::layout {
// stores the connection info between two ranks of nodes in a format fit for efficient intersection count computation
struct ConnectionMat {
    // boolean array (stores size_t to avoid casting around in the prefix sum)
    std::vector<size_t> m_data;
    size_t m_w{}, m_h{};
    bool m_is_inactive{};

    void populate(const Digraph &dg, size_t rank);

    [[nodiscard]] ssize_t getRowLayoutPadding() const;

    [[nodiscard]] size_t getSumDX() const;

    [[nodiscard]] size_t getSumDXAt(size_t offset, ssize_t row_pixel_padding, bool is_column_sum) const;

    void swapNodes(size_t a, size_t b, bool is_column_swap);
};

struct IntersectionMat {
    std::vector<size_t> m_data;
    size_t m_w{}, m_h{};
    // flag set for assertions when the m_data vector is re-used for intermediate results required for updating the
    // total intersection count. Since the intersection_mat is only needed for computing the initial intersection count,
    // this is safe to do, however, I want to save myself potential future headaches and invalidate it explicitly.
    bool m_is_invalidated{};

    void populate(const ConnectionMat &connection_mat);

    [[nodiscard]] size_t getTotalIntersections() const;
};

void clearGlobalState();

void populateOrderingIndexAtRank(Digraph &dg, size_t rank);

void populateInitialOrderings(Digraph &dg);


using BarycenterSweepOperatorFunc = std::function<void(Digraph &, const std::vector<float> &,
                                                       const std::vector<float> &, size_t, bool &)>;

void barycenterSweep(Digraph &dg, bool is_downward_sweep, bool &improvement_found, float &total_change,
                     const BarycenterSweepOperatorFunc &sweep_operator, bool use_median, float barycenter_dampening,
                     bool consider_node_widths);

float getRankOrderingScore(const Digraph &dg, size_t rank);

float updateRankOrderingScoreAfterSwap(size_t idx_a, size_t idx_b);

void revertToPreSwapState(const size_t pre_swap_n_intersections_pr[2], const size_t pre_swap_sum_dx_pr[2], size_t idx_a,
                          size_t idx_b);

// we want to reuse the underlying allocated memory (these vars are cleared every time they are used, this is safe).
// Also, we have 2 because one is for upward and one for downward connections
extern ConnectionMat g_connection_mats[2];
extern IntersectionMat g_intersection_mats[2];
// TODO I should refactor this to use local variables and references instead of globals
extern size_t g_n_intersections_pr[2];
extern size_t g_sum_dx_pr[2];
}
