#include <iostream>

int main() {
    // TODO
    // TODO I have node order computation kinda working, so now, when I have graph layout computation, I should decide
    // whether I want another x position optimization step that optimizes the gaps between nodes (without changing their
    // order) in a sort of physics sim with moving boxes thing? sounds like a bad idea but is it? I could also do a
    // greedy approach where I go from the leftmost and rightmost nodes inward (double pointer style), barycenter-like
    // moving each node and if it moves left (for the left pointer) or right (for right pointer), I can make more room
    // for the next nodes and continue going inward. On the other hand, maybe I want even spacing. So maybe NOT.
    // TODO maybe I should eventually rename the namespace `dot` to `punkt`
    // TODO when different nodes within the same rank have different heights, I need to handle that so no edges go
    // through nodes. I should probably handle this by going up straight until an invisible line that is placed at the
    // vertical rank borders, i.e. at the top and bottom height of the vertically biggest node, i.e. the highest point
    // in the rank. Until there, I should go straight up, then I can go in a straight line (but sideways) towards the
    // point where I want the other end of the edge to be and then I should again go straight up(/down) until I reach
    // the node to connect to.
    // TODO handle subgraphs and clusters
    // TODO handle edge labels (don't forget to update m_rank_counts)
    // TODO handle font loading manually - write a minimal TTF parser and renderer
    // TODO maybe I can do gpu side font rendering? (because I can or something)
    // TODO handle DPI (dots per inch, i.e. different sized screens) properly
    // TODO handle more shapes for nodes
    std::cout << "hello world\n";
}
