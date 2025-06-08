#include "punkt/dot.hpp"
#include "punkt/utils/int_types.hpp"
#include "punkt/dot_constants.hpp"

#include <ranges>

using namespace punkt;

float punkt::BARYCENTER_ORDERING_FADEOUT = 1.0f;
float punkt::BARYCENTER_ORDERING_DAMPENING = 0.6f;
// currently disabled because it doesn't converge consistently
float punkt::BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED = 0.1f;
// TODO re-enable to 5
ssize_t punkt::BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION = 0;

float punkt::BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT = 1.0f;
float punkt::BUBBLE_ORDERING_DX_WEIGHT = 1.0f;
// TODO re-enable to 100
ssize_t punkt::BUBBLE_ORDERING_MAX_ITERS = 0;

// when to stop because change is too insignificant
float punkt::BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED = 0.0f;
// lerps x with mean x
bool punkt::BARYCENTER_X_OPTIMIZATION_REGULARIZATION_ONLY_ON_DOWNWARD = false;
float punkt::BARYCENTER_X_OPTIMIZATION_GHOST_NODE_RELATIVE_WEIGHT = 0.1f;
ssize_t punkt::BARYCENTER_X_OPTIMIZATION_REGULARIZATION_RANK = -1;

// TODO fix the pipeline
// constexpr float x_optimization_pipeline_center_reg_factor = 100.0f;
// constexpr float x_optimization_pipeline_explosion_factor = 50.0f;
// constexpr float x_optimization_pipeline_implosion_factor = 25.0f;
// constexpr int n_implosion_max_iters = 10;
constexpr float x_optimization_pipeline_center_reg_factor = 1000.0f;
constexpr float x_optimization_pipeline_explosion_factor = 2.0f;
std::vector<XOptPipelineStageSettings> punkt::BARYCENTER_X_OPTIMIZATION_PIPELINE = {
    // center
    // XOptPipelineStageSettings{
    //     1, 1, 0.0f, 1.0f, 0.0f, 1.0f / x_optimization_pipeline_center_reg_factor,
    //     SweepMode::sweep_direction_is_innermost_loop,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::after_pipeline_stage,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, false
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{false, false},
    //         XOptPipelineStageSettings::SweepSettings{true, false, 1},
    //     }
    // },
    // first, pull related nodes together so the "explode" step below explodes meaningfully
    // TODO comment back in - this is part of the kinda working small pipeline
    // XOptPipelineStageSettings{
    //     1, 1, 1.0f, 1.0f, 0.0f, 1.0f, SweepMode::normal,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::none,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, true
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{false, false},
    //         XOptPipelineStageSettings::SweepSettings{true, false},
    //     }
    // },
    // explode (increase distance to mean by some factor)
    // TODO comment back in - this is part of the kinda working small pipeline
    // XOptPipelineStageSettings{
    //     1, 1, 0.0f, 1.0f, 0.0f, x_optimization_pipeline_explosion_factor, SweepMode::sweep_direction_is_innermost_loop,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::after_pipeline_stage,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, false
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{false, false},
    //         XOptPipelineStageSettings::SweepSettings{true, false, 1},
    //     }
    // },
    // pull related nodes together again so they are touching
    // XOptPipelineStageSettings{
    //     1, 1, 1.0f, 1.0f, 0.0f, 1.0f, SweepMode::sweep_direction_is_innermost_loop,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::in_barycenter_operator,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction::getExplodeInterGroupNodeSep(
    //             x_optimization_pipeline_explosion_factor),
    //         true
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{false, false},
    //         XOptPipelineStageSettings::SweepSettings{true, false},
    //     }
    // },
    // XOptPipelineStageSettings{
    //     1, 1, 1.0f, 1.0f, 0.0f, 1.0f, SweepMode::sweep_direction_is_innermost_loop,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::in_barycenter_operator,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction::getExplodeInterGroupNodeSep(
    //             x_optimization_pipeline_explosion_factor),
    //         false
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{true, false},
    //         XOptPipelineStageSettings::SweepSettings{true, false},
    //     }
    // },
    // now, groups of nodes that belong together should be together and others should be somewhere else -> group pass
    // TODO comment back in - this is part of the kinda working small pipeline
    // XOptPipelineStageSettings{
    //     1, 1, 1.0f, 1.0f, 0.0f, 1.0f, SweepMode::normal,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::in_barycenter_operator,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, true
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{false, true},
    //         XOptPipelineStageSettings::SweepSettings{true, true},
    //     }
    // },
    // implode (reduce distance to mean by some factor)
    // XOptPipelineStageSettings{
    //     1, 1, 1.0f, 1.0f, 0.0f, 1.0f / x_optimization_pipeline_implosion_factor, SweepMode::normal,
    //     XOptPipelineStageSettings::LegalizerSettings{
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::in_barycenter_operator,
    //         XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, true
    //     },
    //     {
    //         XOptPipelineStageSettings::SweepSettings{},
    //     }
    // },
    // slowly optimize some more
    // TODO test this further
    XOptPipelineStageSettings{
        1, 3, 0.9f, 0.96f, 0.0f, 1.0f, SweepMode::normal,
        XOptPipelineStageSettings::LegalizerSettings{
            XOptPipelineStageSettings::LegalizerSettings::LegalizationTiming::after_iteration,
            XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction{}, false
        },
        {
            XOptPipelineStageSettings::SweepSettings{false, true},
            XOptPipelineStageSettings::SweepSettings{false, false},
            XOptPipelineStageSettings::SweepSettings{true, true},
            XOptPipelineStageSettings::SweepSettings{true, false},
        }
    },
};

void Digraph::preprocess(render::glyph::GlyphLoader &glyph_loader, const std::string_view id_in_parent) {
    if (m_nodes.empty()) {
        return;
    }

    for (Digraph &cluster_dg: std::views::values(m_clusters)) {
        cluster_dg.m_parent = this;
    }

    populateIngoingNodesVectors();
    fuseClusterLinksIntoClusterSuperNodes();
    // fusing cluster links invalidates the Node&'s
    deleteIngoingNodesVectors();

    populateIngoingNodesVectors();
    computeRanks();
    convertParentLinksToIOPorts(id_in_parent);
    // delete the ingoing nodes vectors now because inserting IO ports has invalidated the Node&'s
    deleteIngoingNodesVectors();

    // ghost nodes decompose edges spanning multiple ranks (or 0 ranks) into multiple edges each spanning 1 rank
    insertGhostNodes();

    // per rank reordering of nodes for crossover and edge length minimization
    computeHorizontalOrderings();

    populateIngoingNodesVectors();

    // compute graph layout
    computeNodeLayouts(glyph_loader);
    computeGraphLayout(glyph_loader);
    optimizeGraphLayout();

    // after first optimization run, preprocess the clusters to compute their sizes
    for (auto &[cluster_id, cluster_dg]: m_clusters) {
        cluster_dg.preprocess(glyph_loader, cluster_id);
        // update the cluster node with the cluster dimensions
        Node &node = m_nodes.at(cluster_id);
        node.m_render_attrs.m_width = cluster_dg.m_render_attrs.m_graph_width;
        node.m_render_attrs.m_height = cluster_dg.m_render_attrs.m_graph_height;
    }

    if (!m_clusters.empty()) {
        // re-run x opt after
        optimizeGraphLayout();
    }

    computeEdgeLayout();
    computeEdgeLabelLayouts(glyph_loader);
}
