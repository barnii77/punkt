#include "punkt/dot.hpp"
#include "punkt/int_types.hpp"

using namespace punkt;

float BARYCENTER_ORDERING_DAMPENING = 0.6f;
// currently disabled because it doesn't converge consistently
float BARYCENTER_MIN_AVERAGE_CHANGE_REQUIRED = 0.1f;
ssize_t BARYCENTER_ORDERING_MAX_ITERS_PER_DIRECTION = 5;

float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT = 1.0f;
float BUBBLE_ORDERING_DX_WEIGHT = 1.0f;
ssize_t BUBBLE_ORDERING_MAX_ITERS = 100;

void Digraph::preprocess(render::glyph::GlyphLoader &glyph_loader) {
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
    computeGraphLayout();
    computeEdgeLayout();
}
