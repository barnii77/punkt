#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/utils/int_types.hpp"
#include "punkt/layout/common.hpp"

#include <cassert>
#include <vector>
#include <ranges>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string_view>

using namespace punkt;
using namespace punkt::layout;

void layout::populateOrderingIndexAtRank(Digraph &dg, const size_t rank) {
    for (size_t x = 0; x < dg.m_per_rank_orderings[rank].size(); x++) {
        dg.m_per_rank_orderings_index.at(rank).insert_or_assign(dg.m_per_rank_orderings[rank][x], x);
    }
}

// populates the dg.m_per_rank_orderings by sorting nodes of each rank alphabetically. This is supposed to make similar
// inputs more coherent in terms of output.
void layout::populateInitialOrderings(Digraph &dg) {
    for (const Node &node: std::views::values(dg.m_nodes)) {
        dg.m_per_rank_orderings.at(node.m_render_attrs.m_rank).emplace_back(node.m_name);
    }
    for (auto &ordering: dg.m_per_rank_orderings) {
        std::ranges::sort(ordering);
    }
    // write to dg.m_per_rank_orderings_index maps
    for (size_t rank = 0; rank < dg.m_per_rank_orderings.size(); rank++) {
        populateOrderingIndexAtRank(dg, rank);
    }
}

std::unordered_map<uintptr_t, size_t> g_edge_weight_attribute_cache;

// why is this not the constructor? simple: I want to reuse m_data and save allocations
void ConnectionMat::populate(const Digraph &dg, const size_t rank) {
    assert(rank < dg.m_rank_counts.size() - 1 && dg.m_per_rank_orderings_index.size() == dg.m_rank_counts.size());
    const size_t w = dg.m_rank_counts.at(rank + 1), h = dg.m_rank_counts.at(rank);

    // clear and resize (resize also resets data to false)
    m_data.clear();
    m_data.resize(w * h);
    m_w = w;
    m_h = h;

    // populate
    for (const std::string_view &node_name: std::views::keys(dg.m_per_rank_orderings_index.at(rank))) {
        if (const Node &node = dg.m_nodes.at(node_name); node.m_render_attrs.m_rank == rank) {
            for (const Edge &edge: node.m_outgoing) {
                if (dg.m_nodes.at(edge.m_dest).m_render_attrs.m_rank == rank + 1) {
                    const size_t source_idx = dg.m_per_rank_orderings_index.at(rank).at(edge.m_source);
                    const size_t dest_idx = dg.m_per_rank_orderings_index.at(rank + 1).at(edge.m_dest);
                    assert(source_idx < h && dest_idx < w);
                    const auto edge_ptr = reinterpret_cast<uintptr_t>(&edge);
                    if (!g_edge_weight_attribute_cache.contains(edge_ptr)) {
                        const bool constraint = getAttrOrDefault(edge.m_attrs, "constraint", "true") == "true";
                        g_edge_weight_attribute_cache[edge_ptr] = getAttrTransformedCheckedOrDefault(
                            edge.m_attrs, "weight", constraint ? 1 : 0, stringViewToSizeT);
                    }
                    m_data.at(source_idx * w + dest_idx) += g_edge_weight_attribute_cache.at(edge_ptr);
                }
            }
        }
    }
}

size_t ConnectionMat::getSumDXAt(const size_t offset, ssize_t row_pixel_padding, const bool is_column_sum) const {
    const auto signed_offset = static_cast<ssize_t>(offset);
    size_t limit, offset_stride, i_stride;
    if (is_column_sum) {
        limit = m_h;
        offset_stride = 1;
        i_stride = m_w;
    } else {
        limit = m_w;
        offset_stride = m_w;
        i_stride = 1;
    }
    if (is_column_sum) {
        row_pixel_padding = -row_pixel_padding;
    }

    size_t out = 0;
    for (ssize_t i = 0; i < limit; i++) {
        const ssize_t dx = std::abs(signed_offset - i - row_pixel_padding);
        out += dx * static_cast<ssize_t>(m_data.at(signed_offset * offset_stride + i * i_stride));
    }
    return out;
}

static ssize_t getRowLayoutPaddingForRanks(const size_t node_count_current_rank, const size_t node_count_next_rank) {
    // padding accounts for the fact nodes will be centered. E.g. if there is 1 node in rank 0 and 3 nodes in rank 1,
    // then node 0 in rank 0 will actually be aligned with node 1 in rank 1, meaning dist_r0n0_r1n1 = 0 and
    // dist_r0n0_r1n{0|2} = 1. Graphically, padding is a heuristic for how far rank `rank + 1` will be right shifted
    // compared to rank `rank` for centering purposes.
    return (static_cast<ssize_t>(node_count_current_rank) - static_cast<ssize_t>(node_count_next_rank)) / 2;
}

ssize_t ConnectionMat::getRowLayoutPadding() const {
    return getRowLayoutPaddingForRanks(m_h, m_w);
}


// computes the sum of all x distances between connected node pairs
size_t ConnectionMat::getSumDX() const {
    size_t out = 0;
    const ssize_t padding = getRowLayoutPadding();
    for (ssize_t r = 0; r < m_h; r++) {
        out += getSumDXAt(r, padding, false);
    }
    return out;
}

static void swapArrays(size_t *a, size_t *b, const size_t n, const ssize_t stride) {
    for (size_t i = 0, offset = 0; i < n; i++, offset += stride) {
        std::swap(a[offset], b[offset]);
    }
}

void ConnectionMat::swapNodes(const size_t a, const size_t b, const bool is_column_swap) {
    size_t outer_stride, inner_stride, n;
    if (is_column_swap) {
        outer_stride = 1;
        inner_stride = m_w;
        n = m_h;
    } else {
        outer_stride = m_w;
        inner_stride = 1;
        n = m_w;
    }
    swapArrays(m_data.data() + a * outer_stride, m_data.data() + b * outer_stride, n,
               static_cast<ssize_t>(inner_stride));
}

static size_t reduceSum(const size_t *in, const size_t stride, const size_t n) {
    size_t s = 0;
    for (size_t i = 0; i < n; i++) {
        s += in[i * stride];
    }
    return s;
}

static float medianBarycenterX(const float *barycenters, const size_t *conns, const size_t stride, const size_t n,
                               const float default_value) {
    assert(n > 0);
    const size_t n_elems = reduceSum(conns, stride, n);
    if (n_elems == 0) {
        return default_value;
    }
    const size_t median_idx = (n_elems + 1) / 2;
    size_t i = 0;
    for (size_t n_ones_encountered = 0; n_ones_encountered < median_idx; i++) {
        assert(i < n);
        const size_t value = conns[i * stride];
        n_ones_encountered += value;
    }
    assert(i > 0);
    float median = barycenters[--i];
    if (n_elems % 2 == 0) {
        // we need to average
        while (conns[i * stride] == 0) {
            assert(i < n);
            i++;
        }
        assert(conns[i * stride]);
        median = (median + barycenters[i]) / 2;
    }
    return median;
}

static float meanBarycenterX(const float *barycenters, const size_t *in, const size_t stride, const size_t n,
                             const float default_value) {
    assert(n > 0);
    const size_t n_elems = reduceSum(in, stride, n);
    if (n_elems == 0) {
        return default_value;
    }
    float out = 0;
    for (size_t i = 0; i < n; i++) {
        const size_t value = in[i * stride];
        out += barycenters[i] * static_cast<float>(value);
    }
    return out / static_cast<float>(n_elems);
}

// computes the prefix sum and stores it into out. Assumes in and out have same layout.
static void prefixSum(const size_t *in, size_t *out, const ssize_t stride, const size_t n) {
    size_t s = 0;
    for (size_t i = 0; i < n; i++) {
        // first fetch in_i because in and out may alias
        const size_t in_i = in[i * stride];
        out[i * stride] = s;
        s += in_i;
    }
}

// why is this not the constructor? See ConnectionMat::populate
void IntersectionMat::populate(const ConnectionMat &connection_mat) {
    m_is_invalidated = false;
    // resize to size of connection_mat
    const size_t size = connection_mat.m_w * connection_mat.m_h;
    m_data.clear();
    m_data.resize(size);
    m_w = connection_mat.m_w;
    m_h = connection_mat.m_h;

    // populate prefix sums (this computes number of intersections)
    for (size_t r = 0; r < m_h; r++) {
        prefixSum(connection_mat.m_data.data() + r * m_w, m_data.data() + r * m_w, 1, m_w);
    }
    const size_t last_row_offset = (m_h - 1) * m_w;
    for (size_t c = 0; c < m_w; c++) {
        prefixSum(m_data.data() + c + last_row_offset, m_data.data() + c + last_row_offset,
                  -static_cast<ssize_t>(m_w), m_h); // stride is negative because we want bottom-to-top prefix sum
    }
    // zero out edges that do not exist after the prefix sum computations (they were used for prefix sum propagation, therefore not zero)
    for (size_t i = 0; i < m_w * m_h; i++) {
        m_data[i] *= connection_mat.m_data[i];
    }
}

size_t IntersectionMat::getTotalIntersections() const {
    assert(!m_is_invalidated);
    return std::accumulate(m_data.begin(), m_data.end(), 0ull);
}

ConnectionMat layout::g_connection_mats[2]{};
IntersectionMat layout::g_intersection_mats[2]{};
size_t layout::g_n_intersections_pr[2]{};
size_t layout::g_sum_dx_pr[2]{};

void layout::clearGlobalState() {
    // TODO refactor this - there shouldn't be global state
    g_edge_weight_attribute_cache.clear();
    std::memset(g_n_intersections_pr, 0, sizeof(g_n_intersections_pr));
    std::memset(g_sum_dx_pr, 0, sizeof(g_sum_dx_pr));
    for (size_t i = 0; i < 2; i++) {
        g_connection_mats[i] = ConnectionMat();
        g_intersection_mats[i] = IntersectionMat();
    }
}

// a bit of a weird helper function (computes two values used for rating jointly)
static void computeNumIntersectionsAndSumDX(const Digraph &dg, const size_t rank, const bool is_downward) {
    const auto i = static_cast<size_t>(is_downward);
    layout::g_connection_mats[i].populate(dg, rank);
    layout::g_sum_dx_pr[i] += layout::g_connection_mats[i].getSumDX();
    layout::g_intersection_mats[i].populate(layout::g_connection_mats[i]);
    layout::g_n_intersections_pr[i] += layout::g_intersection_mats[i].getTotalIntersections();
}

static float getWeightedScore(const size_t sum_dx, const size_t n_intersections) {
    return BUBBLE_ORDERING_DX_WEIGHT * static_cast<float>(sum_dx) +
           BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT * static_cast<float>(n_intersections);
}

// computes rank ordering scores between `rank` and `rank + 1`
static float getPartialRankOrderingScore(const Digraph &dg, const size_t rank, const bool is_downward) {
    assert(rank < dg.m_per_rank_orderings.size() - 1);
    const auto i = static_cast<size_t>(is_downward);
    computeNumIntersectionsAndSumDX(dg, rank, is_downward);
    return getWeightedScore(layout::g_sum_dx_pr[i], layout::g_n_intersections_pr[i]);
}

// computes a score (WARNING: smaller is better!) which ranks how good the ordering at rank `rank` is based on
// intersections and connected node distances and takes into account the ranks `rank - 1`, `rank` and `rank + 1`.
float layout::getRankOrderingScore(const Digraph &dg, const size_t rank) {
    assert(rank < dg.m_per_rank_orderings.size());
    g_connection_mats[0].m_is_inactive = rank == 0;
    g_connection_mats[1].m_is_inactive = rank == dg.m_per_rank_orderings.size() - 1;
    for (size_t i = 0; i < 2; i++) {
        g_sum_dx_pr[i] = 0;
        g_n_intersections_pr[i] = 0;
    }
    return (rank == 0 ? 0.0f : getPartialRankOrderingScore(dg, rank - 1, false)) +
           (rank == dg.m_per_rank_orderings.size() - 1 ? 0.0f : getPartialRankOrderingScore(dg, rank, true));
}

static size_t dotProduct(const size_t *a, const size_t *b, const ssize_t stride, const size_t n) {
    size_t out = 0;
    for (size_t i = 0; i < n; i++) {
        const size_t offset = i * stride;
        out += a[offset] * b[offset];
    }
    return out;
}

static size_t computeIntersectionsAB(const ConnectionMat &connection_mat, IntersectionMat &intersection_mat,
                                     const size_t idx_a, const size_t idx_b, const bool is_downward) {
    intersection_mat.m_is_invalidated = true;
    size_t inner_stride, outer_stride, n;
    if (is_downward) {
        inner_stride = 1;
        outer_stride = connection_mat.m_w;
        n = connection_mat.m_w;
    } else {
        inner_stride = connection_mat.m_w;
        outer_stride = 1;
        n = connection_mat.m_h;
    }
    // use intersection_mat.m_data as a temporary buffer (it is reset once it's needed again)
    prefixSum(connection_mat.m_data.data() + outer_stride * idx_b,
              intersection_mat.m_data.data(), static_cast<ssize_t>(inner_stride), n);
    const size_t n_ab_intersections = dotProduct(connection_mat.m_data.data() + outer_stride * idx_a,
                                                 intersection_mat.m_data.data(),
                                                 static_cast<ssize_t>(inner_stride), n);
    return n_ab_intersections;
}

float layout::updateRankOrderingScoreAfterSwap(const size_t idx_a, const size_t idx_b) {
    for (size_t i = 0; i < 2; i++) {
        ConnectionMat &connection_mat = g_connection_mats[i];
        if (connection_mat.m_is_inactive) {
            continue;
        }
        IntersectionMat &intersection_mat = g_intersection_mats[i];
        // get the partial score before swap
        const ssize_t padding = connection_mat.getRowLayoutPadding();
        const size_t orig_sdx_a = connection_mat.getSumDXAt(idx_a, padding, i == 0);
        const size_t orig_sdx_b = connection_mat.getSumDXAt(idx_b, padding, i == 0);
        const size_t orig_intersections =
                computeIntersectionsAB(connection_mat, intersection_mat, idx_a, idx_b, i == 1);

        // get the partial score after swap (for updating the total score)
        connection_mat.swapNodes(idx_a, idx_b, i == 0);
        const size_t new_sdx_a = connection_mat.getSumDXAt(idx_a, padding, i == 0);
        const size_t new_sdx_b = connection_mat.getSumDXAt(idx_b, padding, i == 0);

        g_sum_dx_pr[i] += static_cast<ssize_t>(new_sdx_a + new_sdx_b) - static_cast<ssize_t>(orig_sdx_a + orig_sdx_b);
        const size_t new_intersections =
                computeIntersectionsAB(connection_mat, intersection_mat, idx_a, idx_b, i == 1);
        g_n_intersections_pr[i] += static_cast<ssize_t>(new_intersections) - static_cast<ssize_t>(orig_intersections);
    }

    return getWeightedScore(g_sum_dx_pr[0], g_n_intersections_pr[0]) +
           getWeightedScore(g_sum_dx_pr[1], g_n_intersections_pr[1]);
    // return getRankOrderingScore(dg, rank);
}

void layout::revertToPreSwapState(const size_t pre_swap_n_intersections_pr[2], const size_t pre_swap_sum_dx_pr[2],
                                  const size_t idx_a, const size_t idx_b) {
    for (size_t i = 0; i < 2; i++) {
        g_n_intersections_pr[i] = pre_swap_n_intersections_pr[i];
        g_sum_dx_pr[i] = pre_swap_sum_dx_pr[i];
        if (ConnectionMat &connection_mat = g_connection_mats[i]; !connection_mat.m_is_inactive) {
            connection_mat.swapNodes(idx_a, idx_b, i == 0);
        }
    }
}

void Digraph::swapNodesOnRank(const std::string_view a, const std::string_view b) {
    const size_t rank = m_nodes.at(a).m_render_attrs.m_rank;
    assert(rank == m_nodes.at(b).m_render_attrs.m_rank);

    auto &ordering = m_per_rank_orderings.at(rank);
    auto &ordering_index = m_per_rank_orderings_index.at(rank);

    const size_t a_idx = ordering_index.at(a), b_idx = ordering_index.at(b);
    ordering_index[a] = b_idx, ordering_index[b] = a_idx;

    assert(ordering.at(a_idx) == a && ordering.at(b_idx) == b);
    ordering[a_idx] = b, ordering[b_idx] = a;
}

void layout::barycenterSweep(Digraph &dg, const bool is_downward_sweep, bool &improvement_found, float &total_change,
                             const BarycenterSweepOperatorFunc &sweep_operator, const bool use_median,
                             const float barycenter_dampening, const ssize_t start_rank, const ssize_t n_ranks) {
    // iterates ranks [n - 1, 0) if is_upward_pass else [0, n - 1)
    ssize_t start, end, rank_step;
    if (is_downward_sweep) {
        start = 1 + (start_rank < 0 ? 0 : start_rank);
        end = n_ranks < 0 ? static_cast<ssize_t>(dg.m_per_rank_orderings.size()) : start + n_ranks;
        rank_step = 1;
    } else {
        start = static_cast<ssize_t>(dg.m_per_rank_orderings.size()) - 2 - (start_rank < 0 ? 0 : start_rank);
        end = n_ranks < 0 ? -1 : start - n_ranks;
        rank_step = -1;
    }

    for (ssize_t rank = start; rank != end; rank += rank_step) {
        ConnectionMat &connection_mat = g_connection_mats[static_cast<size_t>(is_downward_sweep)];
        connection_mat.populate(dg, rank - static_cast<size_t>(is_downward_sweep));

        size_t n_barycenters = connection_mat.m_h, inner_dim = connection_mat.m_w,
                outer_stride = connection_mat.m_w, inner_stride = 1;
        if (is_downward_sweep) {
            std::swap(n_barycenters, inner_dim);
            std::swap(outer_stride, inner_stride);
        }

        assert(n_barycenters == dg.m_per_rank_orderings.at(rank).size());
        assert(inner_dim == dg.m_per_rank_orderings.at(rank - rank_step).size());

        std::vector<float> new_barycenters(n_barycenters);
        std::vector<float> old_barycenters(n_barycenters);
        std::vector<float> current_other_rank_barycenters(inner_dim);
        for (size_t i = 0; i < inner_dim; i++) {
            const auto &node_name = dg.m_per_rank_orderings.at(rank - rank_step).at(i);
            const auto &node = dg.m_nodes.at(node_name);
            current_other_rank_barycenters[i] = node.m_render_attrs.m_barycenter_x;
            // const auto width_adjustment = consider_node_widths
            //                                   ? static_cast<float>(node.m_render_attrs.m_width) / 2.0f
            //                                   : 0.0f;
            // current_other_rank_barycenters[i] = node.m_render_attrs.m_barycenter_x + width_adjustment;
        }
        const auto &rank_ordering = dg.m_per_rank_orderings.at(rank);
        for (size_t i = 0; i < n_barycenters; i++) {
            float p;
            const auto &node_name = rank_ordering.at(i);
            auto &node = dg.m_nodes.at(node_name);
            if (use_median) {
                p = medianBarycenterX(current_other_rank_barycenters.data(),
                                      connection_mat.m_data.data() + i * outer_stride,
                                      inner_stride, inner_dim, node.m_render_attrs.m_barycenter_x);
            } else {
                p = meanBarycenterX(current_other_rank_barycenters.data(),
                                    connection_mat.m_data.data() + i * outer_stride,
                                    inner_stride, inner_dim, node.m_render_attrs.m_barycenter_x);
            }
            const float new_barycenter =
                    std::lerp(node.m_render_attrs.m_barycenter_x, p, barycenter_dampening);
            const float change = std::abs(new_barycenter - node.m_render_attrs.m_barycenter_x);
            total_change += change;
            old_barycenters[i] = node.m_render_attrs.m_barycenter_x;
            node.m_render_attrs.m_barycenter_x = new_barycenter;
            new_barycenters[i] = new_barycenter;
        }

        sweep_operator(dg, std::move(new_barycenters), std::move(old_barycenters), rank, improvement_found);
    }
}
