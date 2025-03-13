#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"

#include <cassert>
#include <vector>
#include <ranges>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <string_view>

// TODO at some point I could implement this (if I find a case where the current method is not sufficient):
// try to produce good solutions through evolution on the graphs where the inverse fitness function is my
// score, and you have a pool of graphs of size `N` (constant size) that are evolved in outer and inner steps:
// Each outer step (evolution step) you combine the previous evolution step graphs and the new ones into one pool,
// pick the top `T` graphs out of there and put them into the next step pool (so that the top few graphs always survive).
// Then, you find the max score, invert all scores around that (i.e. score_inv = max_score - score) and then use those
// as relative weights for reproduction. You then fill up the pool to max size by sampling randomly with those weights.
// You repeat the above for `E` (epochs) steps.
// Now, how does the inner step work? Each inner step will perform `I` random swaps in random ranks (i.e. random
// mutations).
// For initialization, we start with `N` randomly permuted graphs to get lots of genetic diversity in the beginning.
// Except they won't be completely random. I will actually throw the randomly permuted graphs into a barycenter algo
// first, which is what graphviz does, and that way I hope to produce reasonable starting points.

using namespace punkt;

static void populateOrderingIndexAtRank(Digraph &dg, const size_t rank) {
    for (size_t x = 0; x < dg.m_per_rank_orderings[rank].size(); x++) {
        dg.m_per_rank_orderings_index.at(rank).emplace(dg.m_per_rank_orderings[rank][x], x);
    }
}

// populates the dg.m_per_rank_orderings by sorting nodes of each rank alphabetically. This is supposed to make similar
// inputs more coherent in terms of output.
static void populateInitialOrderings(Digraph &dg) {
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

// stores the connection info between two ranks of nodes in a format fit for efficient intersection count computation
struct ConnectionMat {
    // boolean array (stores size_t to avoid casting around in the prefix sum)
    std::vector<size_t> m_data;
    size_t m_w{}, m_h{};

    void populate(const Digraph &dg, size_t rank);

    [[nodiscard]] size_t getSumDX() const;
};

struct IntersectionMat {
    std::vector<size_t> m_data;
    size_t m_w{}, m_h{};

    void populate(const ConnectionMat &connection_mat);

    [[nodiscard]] size_t getTotalIntersections() const;
};

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
                    m_data.at(source_idx * w + dest_idx) = 1;
                }
            }
        }
    }
}

// computes the sum of all x distances between connected node pairs
size_t ConnectionMat::getSumDX() const {
    size_t out = 0;

    // padding accounts for the fact nodes will be centered. E.g. if there is 1 node in rank 0 and 3 nodes in rank 1,
    // then node 0 in rank 0 will actually be aligned with node 1 in rank 1, meaning dist_r0n0_r1n1 = 0 and
    // dist_r0n0_r1n{0|2} = 1. Graphically, padding is a heuristic for how far rank `rank + 1` will be right shifted
    // compared to rank `rank` for centering purposes.
    const ssize_t padding = (static_cast<ssize_t>(m_h) - static_cast<ssize_t>(m_w)) / 2;

    for (ssize_t r = 0; r < m_h; r++) {
        for (ssize_t c = 0; c < m_w; c++) {
            if (m_data.at(r * m_w + c) == 1) {
                ssize_t dx = std::abs(r - c - padding);
                out += dx;
            }
        }
    }

    return out;
}

static size_t reduceSum(const size_t *in, const size_t stride, const size_t n) {
    size_t s = 0;
    for (size_t i = 0; i < n; i++) {
        s += in[i * stride];
    }
    return s;
}

static float medianBarycenterX(const float *barycenters, const size_t *in, const size_t stride, const size_t n,
                               const float default_value) {
    assert(n > 0);
    const size_t n_elems = reduceSum(in, stride, n);
    if (n_elems == 0) {
        return default_value;
    }
    const size_t median_idx = (n_elems + 1) / 2;
    size_t i = 0;
    for (size_t n_ones_encountered = 0; n_ones_encountered < median_idx; i++) {
        assert(i < n);
        const size_t value = in[i * stride];
        assert(value < 2);
        n_ones_encountered += value;
    }
    assert(i > 0);
    float median = barycenters[--i];
    if (n_elems % 2 == 0) {
        // we need to average
        while (in[i * stride] == 0) {
            assert(i < n);
            i++;
        }
        assert(in[i * stride] == 1);
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
        assert(value < 2);
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
    return std::accumulate(m_data.begin(), m_data.end(), 0ull);
}

// we want to reuse the underlying allocated memory (these vars are cleared every time they are used, this is safe)
static ConnectionMat connection_mat;
static IntersectionMat intersection_mat;

// a bit of a weird helper function (computes two values used for rating jointly)
static void getNumIntersectionsAndSumDX(const Digraph &dg, const size_t rank, size_t &n_intersections, size_t &sum_dx) {
    // TODO one could eventually implement a reuse mechanism where not all of connection_mat and intersection_mat are
    // recomputed, instead recomputing only parts
    connection_mat.populate(dg, rank);
    sum_dx = connection_mat.getSumDX();
    intersection_mat.populate(connection_mat);
    n_intersections = intersection_mat.getTotalIntersections();
}

// computes rank ordering scores between `rank` and `rank + 1`
static float getPartialRankOrderingScore(const Digraph &dg, const size_t rank) {
    assert(rank < dg.m_per_rank_orderings.size() - 1);
    size_t sum_dx, n_intersections;
    getNumIntersectionsAndSumDX(dg, rank, n_intersections, sum_dx);
    return BUBBLE_ORDERING_DX_WEIGHT * static_cast<float>(sum_dx) +
           BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT * static_cast<float>(n_intersections);
}

// computes a score (WARNING: smaller is better!) which ranks how good the ordering at rank `rank` is based on
// intersections and connected node distances and takes into account the ranks `rank - 1`, `rank` and `rank + 1`.
static float getRankOrderingScore(const Digraph &dg, const size_t rank) {
    assert(rank < dg.m_per_rank_orderings.size());
    return (rank == 0 ? 0.0f : getPartialRankOrderingScore(dg, rank - 1)) +
           (rank == dg.m_per_rank_orderings.size() - 1 ? 0.0f : getPartialRankOrderingScore(dg, rank));
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

static void barycenterSweep(Digraph &dg, const bool is_downward_sweep, bool &improvement_found, float &total_change) {
    // iterates ranks [n - 1, 0) if is_upward_pass else [0, n - 1)
    ssize_t start, end;
    int rank_step;
    if (is_downward_sweep) {
        start = 1;
        end = static_cast<ssize_t>(dg.m_per_rank_orderings.size());
        rank_step = 1;
    } else {
        start = static_cast<ssize_t>(dg.m_per_rank_orderings.size()) - 2;
        end = -1;
        rank_step = -1;
    }

    for (ssize_t rank = start; rank != end; rank += rank_step) {
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
        std::vector<float> current_other_rank_barycenters(inner_dim);
        for (size_t i = 0; i < inner_dim; i++) {
            const auto &node_name = dg.m_per_rank_orderings.at(rank - rank_step).at(i);
            const auto &node = dg.m_nodes.at(node_name);
            current_other_rank_barycenters[i] = node.m_render_attrs.m_barycenter_x;
        }
        for (size_t i = 0; i < n_barycenters; i++) {
            float p;
            const auto &node_name = dg.m_per_rank_orderings.at(rank).at(i);
            auto &node = dg.m_nodes.at(node_name);
            if (BARYCENTER_USE_MEDIAN) {
                p = medianBarycenterX(current_other_rank_barycenters.data(),
                                      connection_mat.m_data.data() + i * outer_stride,
                                      inner_stride, inner_dim, node.m_render_attrs.m_barycenter_x);
            } else {
                p = meanBarycenterX(current_other_rank_barycenters.data(),
                                    connection_mat.m_data.data() + i * outer_stride,
                                    inner_stride, inner_dim, node.m_render_attrs.m_barycenter_x);
            }
            const float new_barycenter =
                    std::lerp(node.m_render_attrs.m_barycenter_x, p, BARYCENTER_ORDERING_DAMPENING);
            const float change = std::abs(new_barycenter - node.m_render_attrs.m_barycenter_x);
            total_change += change;
            node.m_render_attrs.m_barycenter_x = new_barycenter;
            new_barycenters[i] = new_barycenter;
        }

        std::vector<size_t> rearrangement_order(n_barycenters);
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
            continue;
        }

        // rearrange orderings vector according to rearrangement order
        auto &ordering = dg.m_per_rank_orderings.at(rank);
        std::vector<std::string_view> rearranged_ordering;
        rearranged_ordering.reserve(ordering.size());
        for (const size_t idx: rearrangement_order) {
            rearranged_ordering.emplace_back(ordering.at(idx));
        }
        // TODO revert to old medianIndex/meanIndex mechanism if this works (it currently doesn't)
        for (size_t i = 0; i < n_barycenters; i++) {
            const auto &node_name = dg.m_per_rank_orderings.at(rank).at(i);
            auto &node = dg.m_nodes.at(node_name);
            node.m_render_attrs.m_barycenter_x = static_cast<float>(i);
        }

        // const float old_score = getRankOrderingScore(dg, rank);
        std::swap(ordering, rearranged_ordering);
        populateOrderingIndexAtRank(dg, rank);
        improvement_found = true;
        // if (old_score <= getRankOrderingScore(dg, rank)) {
        //     // this reordering makes it worse, undo changes
        //     std::swap(ordering, rearranged_ordering);
        //     populateOrderingIndexAtRank(dg, rank);
        // } else {
        //     improvement_found = true;
        // }
    }
}

void Digraph::computeHorizontalOrderings() {
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

    // TODO barycenter still doesn't converge and just gets stuck... I am probably doing something wrong
    // run median barycenter iterations of alternating up and down sweeps
    for (ssize_t barycenter_iter = 0; barycenter_iter < BARYCENTER_ORDERING_MAX_ITERS ||
                                      BARYCENTER_ORDERING_MAX_ITERS < 0; barycenter_iter++) {
        bool improvement_found = false;
        float total_change = 0.0f;
        for (const bool is_downward_sweep: {false, true}) {
            barycenterSweep(*this, is_downward_sweep, improvement_found, total_change);
        }
        // TODO remove this
        total_change = 0.0f;
        if (const float average_change = total_change / static_cast<float>(m_nodes.size());
            !improvement_found && average_change < BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED *
            BARYCENTER_ORDERING_DAMPENING) {
            break;
        }
    }

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
            for (size_t node_idx = 0; node_idx < m_per_rank_orderings.at(rank).size() - 1; node_idx++) {
                // attempt swapping node with neighbour
                swapNodesOnRank(m_per_rank_orderings[rank][node_idx], m_per_rank_orderings[rank][node_idx + 1]);

                if (const float new_rank_score = getRankOrderingScore(*this, rank);
                    new_rank_score < rank_scores[rank]) {
                    rank_scores[rank] = new_rank_score;
                    improvement_found = true;
                } else {
                    // revert the change
                    swapNodesOnRank(m_per_rank_orderings[rank][node_idx], m_per_rank_orderings[rank][node_idx + 1]);
                }
            }
        }
        if (!improvement_found) {
            break;
        }
    }
}
