#pragma once

constexpr auto KWD_NODE = "node";
constexpr auto KWD_EDGE = "edge";
constexpr auto KWD_DIGRAPH = "digraph";
constexpr auto KWD_GRAPH = "graph";

// technically, these vars aren't constants, but they act like constants because they are set once at program start
// through CLI args (or maybe not at all) and then never changed.

extern float BARYCENTER_ORDERING_DAMPENING;
extern ssize_t BARYCENTER_ORDERING_MAX_ITERS;
// relative weight of number of edge crossovers compared to sum of x distances between connected nodes.
// used for scoring node orderings within a rank.
extern float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT;
extern float BUBBLE_ORDERING_DX_WEIGHT;
extern ssize_t BUBBLE_ORDERING_MAX_ITERS;
