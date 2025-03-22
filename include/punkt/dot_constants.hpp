#pragma once

#include "punkt/int_types.hpp"

#include <string_view>

constexpr std::string_view default_shape = "box";

constexpr size_t default_font_size = 14;

constexpr int expected_edge_line_length = 4;

constexpr float arrow_scale = 5.0f;

constexpr auto KWD_NODE = "node";
constexpr auto KWD_EDGE = "edge";
constexpr auto KWD_DIGRAPH = "digraph";
constexpr auto KWD_SUBGRAPH = "subgraph";
constexpr auto KWD_GRAPH = "graph";

// technically, these vars aren't constants, but they act like constants because they are set once at program start
// through CLI args (or maybe not at all) and then never changed.

// decides whether to use median or mean barycenter (constexpr so compiler can do optimization magic)
// NOTE: empirically, median seems to work *way* better than mean
constexpr bool BARYCENTER_USE_MEDIAN = true;
// all this doesn't benefit from being constexpr because it's just magic numbers anyway
extern float BARYCENTER_ORDERING_DAMPENING;
extern float BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED;
extern ssize_t BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION;
// relative weight of number of edge crossovers compared to sum of x distances between connected nodes.
// used for scoring node orderings within a rank.
extern float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT;
extern float BUBBLE_ORDERING_DX_WEIGHT;
extern ssize_t BUBBLE_ORDERING_MAX_ITERS;
constexpr size_t DEFAULT_DPI = 96;
