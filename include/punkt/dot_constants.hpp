#pragma once

#include "punkt/utils/int_types.hpp"

#include <string_view>
#include <vector>

namespace punkt {
constexpr std::string_view default_shape = "box";
constexpr std::string_view default_color = "black";

constexpr size_t default_font_size = 14;

constexpr int expected_edge_line_length = 4;

constexpr float arrow_scale = 5.0f;

// number of straight lines a spline is subdivided into for rendering
constexpr size_t n_spline_divisions = 255;

// When a node has a self-connection (`A -> A`), that gets decomposed into `A -> @num` and `@num -> A`, which is called
// ghost node insertion. After this ghost node insertion pass, all edges have a rank diff between source and dest of
// either -1 or 1. However, when you combine this with spline edges and self-connections, the result doesn't look good.
// We therefore re-coalesce those two edges (`A -> @num` and `@num -> A`) when computing the bezier base points. Now,
// the result will still not look good. That's where this value comes in. It will check the height diff of the edge and
// space out the 2 middle control points of the cubic bezier curve by self_connection_x_broadening_strength * height in
// x direction, which will create a good-looking loop.
constexpr double self_connection_x_broadening_dx_dy_ratio = 0.25;

constexpr auto KWD_NODE = "node";
constexpr auto KWD_EDGE = "edge";
constexpr auto KWD_DIGRAPH = "digraph";
constexpr auto KWD_SUBGRAPH = "subgraph";
constexpr auto KWD_GRAPH = "graph";

// decides whether to use median or mean barycenter (constexpr so compiler can do optimization magic)
// NOTE: empirically, median seems to work *way* better than mean
constexpr bool BARYCENTER_USE_MEDIAN = true;
constexpr bool BARYCENTER_X_OPTIMIZATION_USE_MEDIAN = true;
constexpr bool BARYCENTER_SWEEP_DIRECTION_IS_OUTER_LOOP = false;
constexpr bool BARYCENTER_X_OPTIMIZATION_REORDER_BY_BARYCENTER_X_BEFORE_LEGALIZE = true;

// all this doesn't benefit from being constexpr because it's just magic numbers anyway
extern float BARYCENTER_ORDERING_FADEOUT;
extern float BARYCENTER_ORDERING_DAMPENING;
extern float BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED;
extern ssize_t BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION;
// relative weight of number of edge crossovers compared to sum of x distances between connected nodes.
// used for scoring node orderings within a rank.
extern float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT;
extern float BUBBLE_ORDERING_DX_WEIGHT;
extern ssize_t BUBBLE_ORDERING_MAX_ITERS;
constexpr size_t DEFAULT_DPI = 96;
// X optimization is the process which optimizes the initial x positioning of the nodes while keeping ordering fixed
extern float BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED;
extern ssize_t BARYCENTER_X_OPTIMIZATION_REGULARIZATION_RANK;
extern bool BARYCENTER_X_OPTIMIZATION_REGULARIZATION_ONLY_ON_DOWNWARD;
// weight of ghost nodes relative to normal nodes (all normal nodes have the same weight)
extern float BARYCENTER_X_OPTIMIZATION_GHOST_NODE_RELATIVE_WEIGHT;

enum class SweepMode {
    normal,
    sweep_direction_is_outer_loop,
    sweep_direction_is_innermost_loop,
};

struct XOptPipelineStageSettings {
    size_t m_repeats{};
    ssize_t m_max_iters{};
    // lerps new x with old x for smoother adjustment
    float m_initial_dampening{};
    // fades out the dampening exponentially with this factor
    float m_dampening_fadeout{};
    // pulls toward rank mean
    float m_pull_towards_mean{};
    // pulls toward 0
    float m_regularization{};
    SweepMode m_sweep_mode{};

    struct LegalizerSettings {
        enum class LegalizationTiming {
            none,
            after_pipeline_stage,
            after_iteration,
            in_barycenter_operator,
        };

        struct LegalizerSpecialInstruction {
            enum class Type {
                none,
                explode_inter_group_node_sep,
            };

            Type m_type{Type::none};
            float m_explode_inter_group_node_sep_factor{1.0f};

            static LegalizerSpecialInstruction getExplodeInterGroupNodeSep(float factor);
        };

        LegalizationTiming m_legalization_timing{LegalizationTiming::none};
        LegalizerSpecialInstruction m_legalizer_special_instruction{};
        bool m_try_cancel_mean_shift{};

        LegalizerSettings();

        LegalizerSettings(LegalizationTiming legalization_timing,
                          LegalizerSpecialInstruction legalizer_special_instruction, bool try_cancel_mean_shift);
    };

    LegalizerSettings m_legalizer_settings{};

    struct SweepSettings {
        bool m_is_downward_sweep{}, m_is_group_sweep{};
        ssize_t m_sweep_n_ranks_limit{-1};

        SweepSettings();

        SweepSettings(bool is_downward_sweep, bool is_group_sweep, ssize_t sweep_n_ranks_limit = -1);
    };

    std::vector<SweepSettings> m_sweep_settings;

    XOptPipelineStageSettings();

    XOptPipelineStageSettings(size_t repeats, ssize_t max_iters, float initial_dampening, float dampening_fadeout,
                              float pull_towards_mean, float regularization, SweepMode sweep_mode,
                              LegalizerSettings legalizer_settings, std::vector<SweepSettings> sweep_settings);
};

extern std::vector<XOptPipelineStageSettings> BARYCENTER_X_OPTIMIZATION_PIPELINE;
}
