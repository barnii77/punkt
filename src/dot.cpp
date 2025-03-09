#include "punkt/dot.hpp"

using namespace dot;

float BARYCENTER_ORDERING_DAMPENING = 1.0f;
// currently disabled because it doesn't converge consistently
ssize_t BARYCENTER_ORDERING_MAX_ITERS = 0;

float BUBBLE_ORDERING_CROSSOVER_COUNT_WEIGHT = 1.0f;
float BUBBLE_ORDERING_DX_WEIGHT = 1.0f;
ssize_t BUBBLE_ORDERING_MAX_ITERS = -1;

void Digraph::preprocess() {
    // ingoing nodes vectors are required for computing the ranks
    populateIngoingNodesVectors();
    computeRanks();

    // delete the ingoing nodes vectors now because inserting ghost nodes will invalidate the Node&'s
    deleteIngoingNodesVectors();

    // ghost nodes decompose edges spanning multiple ranks (or 0 ranks) into multiple edges each spanning 1 rank
    insertGhostNodes();

    // per rank reordering of nodes for crossover and edge length minimization
    computeHorizontalOrderings();
}
