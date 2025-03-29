#include "punkt/dot.hpp"
#include "punkt/int_types.hpp"

using namespace punkt;

// TODO find the best values here by tuning the barycenter parameters
float BARYCENTER_ORDERING_FADEOUT = 1.0f;
float BARYCENTER_ORDERING_DAMPENING = 0.6f;
// currently disabled because it doesn't converge consistently
float BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED = 0.1f;
ssize_t BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION = 5;

float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT = 1.0f;
float BUBBLE_ORDERING_DX_WEIGHT = 1.0f;
ssize_t BUBBLE_ORDERING_MAX_ITERS = 100;

// fades out the dampening exponentially with this factor
float BARYCENTER_X_OPTIMIZATION_FADEOUT = 0.7f;
// lerps new x with old x for smoother adjustment
float BARYCENTER_X_OPTIMIZATION_DAMPENING = 1.0f;
// when to stop because change is too insignificant
float BARYCENTER_X_OPTIMIZATION_MIN_AVERAGE_CHANGE_REQUIRED = 0.5f;
// lerps x with mean x
// TODO I have to figure out some other way to make it more stable - this makes big graphs look terrible (see ninja.dot)
float BARYCENTER_X_OPTIMIZATION_PULL_TOWARDS_MEAN = 0.0f;
float BARYCENTER_X_OPTIMIZATION_REGULARIZATION = 1.0f;
ssize_t BARYCENTER_X_OPTIMIZATION_MAX_ITERS = 5;

void Digraph::preprocess(render::glyph::GlyphLoader &glyph_loader, const size_t graph_x, const size_t graph_y) {
    if (m_nodes.empty()) {
        return;
    }

    // ingoing nodes vectors are required for computing the ranks
    populateIngoingNodesVectors();
    computeRanks();

    // delete the ingoing nodes vectors now because inserting ghost nodes will invalidate the Node&'s
    deleteIngoingNodesVectors();

    // ghost nodes decompose edges spanning multiple ranks (or 0 ranks) into multiple edges each spanning 1 rank
    insertGhostNodes();

    // per rank reordering of nodes for crossover and edge length minimization
    computeHorizontalOrderings();

    populateIngoingNodesVectors();
    // compute graph layout
    computeNodeLayouts(glyph_loader);
    computeGraphLayout(glyph_loader, graph_x, graph_y);
    optimizeGraphLayout();
    computeEdgeLayout();
    computeEdgeLabelLayouts(glyph_loader);
}
