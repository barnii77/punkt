#include "punkt/dot.hpp"

using namespace dot;

void Digraph::preprocess() {
    // ingoing nodes vectors are required for computing the ranks
    populateIngoingNodesVectors();
    computeRanks();

    // delete the ingoing nodes vectors now because inserting ghost nodes will invalidate the Node&'s
    deleteIngoingNodesVectors();

    // ghost nodes decompose edges spanning multiple ranks (or 0 ranks) into multiple edges each spanning 1 rank
    insertGhostNodes();
}
