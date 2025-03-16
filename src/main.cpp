#include "punkt/punkt.h"

int main() {
    // TODO handle line thickness
    // TODO in addition to TTF, I should also make my own font format which is a super simple bitmap font format where
    // you just store a bunch of uint8_t's with certain dimensions and an index table at the start of the file.
    // The bitmap is then loaded and just scaled up. I could also make my own image up-scaling for that.
    // TODO set ghost node color to edge color during ghost node creation
    // TODO make sure ghost nodes are pixel perfect aligned with vertices:
        // TODO the way I should probably do that is by forcing edges to be `virtual pixel (dpi)` aligned (i.e.
        // artificial zoom imprecision)
    // TODO edge anti-aliasing
    // TODO limit the character size in the glTexImage2D call to max(width, height) == min(1024, gl_max_texture_size)
    // TODO in the bubble nodes sort maybe I can only apply a tiny diff to the current adjacency matrix and adjust
    // crossover count: O(n^4) -> O(n^2)|O(n^3)?
    // TODO fix cmakelists to make it rebuild on shader change
    // TODO wait, actually, edge labels may be easier than I thought because edges are allowed to clip through edge
    // labels... however, they're not so easy in the sense that I have to modify the way that edge labels are
    // propagated to ghost edges, because I only want the label once, not on every ghost edge
    // TODO handle edge labels (label, headlabel, taillabel) (don't forget to update m_rank_counts)
    // TODO I have node order computation kinda working, so now, when I have graph layout computation, I should decide
    // whether I want another x position optimization step that optimizes the gaps between nodes (without changing their
    // order) in a sort of physics sim with moving boxes thing? sounds like a bad idea but is it? I could also do a
    // greedy approach where I go from the leftmost and rightmost nodes inward (double pointer style), barycenter-like
    // moving each node and if it moves left (for the left pointer) or right (for right pointer), I can make more room
    // for the next nodes and continue going inward. On the other hand, maybe I want even spacing. So maybe NOT.
    // TODO handle arrows
    // TODO if node attr `width` is specified on a node, respect that when layout baking and insert line wraps
    // TODO handle subgraphs and clusters
    // TODO handle font loading manually - write a minimal TTF parser and renderer
    // TODO maybe I can do gpu side font rendering? (because I can or something)
    // TODO handle DPI (dots per inch, i.e. different sized screens) properly
    // TODO handle more shapes for nodes
    // TODO make sure this builds using all major build systems (make, ninja, visual studio msbuild)
    // TODO maybe handle xlabels on edges eventually (external label, i.e. the edge goes through the left edge of the
    // label node instead of the center)
    // TODO handle color gradients on edges
    // TODO maybe I could add drawing on top xD (i.e. you have a separate surface that you draw into in black and that
    // gets rendered last so it overdraws the graph
    // TODO XDD I can add animations because I don't have to comply with the dot language spec exactly, just roughly

    // TODO add font
    // TODO init in the middle of the graph
    static auto test_input = R"(
digraph G {
    node [fillcolor=green];
    A [fontcolor=red, label="hello world!", penwidth=5];
    A -> B [penwidth=5, color=red];
}
)";
    test_input = R"(
        digraph OptimalTest {
            // Declare nodes (order in source is not necessarily preserved, but ranks will be computed)
            A; B; C;
            D; E;
            F; G; H;
            I; J;
            // Edges:
            A -> D;
            B -> D;
            C -> E;
            A -> F;
            D -> G;
            E -> G;
            D -> H;
            B -> I;
            F -> I;
            G -> J;
            H -> J;
            F -> G;
        }
)";
    punktRun(test_input, nullptr);
}
