#include "punkt/punkt.h"

int main() {
    static auto test_input = R"(
digraph G {
    node [fillcolor=green];
    A [fontcolor=red, label="hello world!", penwidth=5];
    A -> B [penwidth=5, color=red];
}
)";
    test_input = R"(
        digraph GraphLayoutTest {
            rankdir=TB;
            ranksep=75;
            nodesep=50;

            A [label="Node A\nFirst rank"];
            B [label="Node B\nFirst rank too"];
            C [label="This is\nNode C\nSecond rank"];
            D [label="D in\nsecond rank"];
            E [label="E in third rank"];
            F [label="F also\nin third rank"];
            G [label="G in fourth"];

            A -> C;
            A -> D;
            B -> C;
            B -> D;
            C -> E;
            D -> F;
            E -> G;
            F -> G;
        }
)";
    test_input = R"(
digraph HugeCyclicGraph {
    // Define nodes
    node [shape=circle];

    // Create a chain of nodes with a few added cycles
    0 -> 1;  1 -> 2;  2 -> 3;  3 -> 4;  4 -> 5;
    5 -> 6;  6 -> 7;  7 -> 8;  8 -> 9;  9 -> 10;
    10 -> 11;  11 -> 12;  12 -> 13;  13 -> 14;  14 -> 15;
    15 -> 16;  16 -> 17;  17 -> 18;  18 -> 19;  19 -> 20;
    20 -> 21;  21 -> 22;  22 -> 23;  23 -> 24;  24 -> 25;
    25 -> 26;  26 -> 27;  27 -> 28;  28 -> 29;

    // Introduce some cycles
    0 -> 10;
    5 -> 15;
    8 -> 18;
    14 -> 24;
    19 -> 9;
    25 -> 5;
    22 -> 17;
    26 -> 7;
}
)";
    test_input = R"(
        digraph OptimalTest {
            // Declare nodes (order in source is not necessarily preserved, but ranks will be computed)
            node [shape=ellipse, fillcolor=red];
            edge [headlabel="HI!", label="HO!", taillabel="HA?", fontsize=5];
            AXYZ; B; C;
            D; E; F;
            G; H; I;
            J;
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
    test_input = R"(
        digraph GraphLayoutTest {
            rankdir=TB;
            ranksep=75;
            nodesep=50;

            A [label="Node A\nFirst rank", fontsize=12];
            B [label="Node B\nFirst rank too"];
            C [label="This is\nNode C\nSecond rank"];
            D [label="D in\nsecond rank", color=green];
            E [label="E in third rank"];
            F [label="F also\nin third rank", fillcolor=red];
            G [label="G in fourth", shape=ellipse, penwidth=5.0];

            A -> C [label="E1"];
            A -> D;
            B -> C [headlabel="E3"];
            B -> D;
            C -> E [taillabel="E5", label="E5"];
            D -> F;
            E -> G;
            F -> G;
            A -> G [headlabel="E42", label="E42", taillabel="E42", fontsize=7];
        }
)";
    const char *font_path = nullptr;
    font_path = "resources/fonts/tinyfont.psf";
    punktRun(test_input, font_path);
}

////////////////////// TODOS //////////////////////

// TODO in the last example there are some (i assume size_t truncating?) imperfections where ghost edges just don't
// match in size and it looks really bad
// TODO handle spline edges
// TODO handle subgraphs and clusters
// TODO I have node order computation kinda working, so now, when I have graph layout computation, I should decide
// whether I want another x position optimization step that optimizes the gaps between nodes (without changing their
// order) in a sort of physics sim with moving boxes thing? sounds like a bad idea but is it? I could also do a
// greedy approach where I go from the leftmost and rightmost nodes inward (double pointer style), barycenter-like
// moving each node and if it moves left (for the left pointer) or right (for right pointer), I can make more room
// for the next nodes and continue going inward. On the other hand, maybe I want even spacing. So maybe NOT.
// TODO handle dashed and dotted edges
// TODO handle DPI properly, currently, 1 dot = 1 pixel, but that's simply not true
// TODO handle font loading manually - write a minimal TTF parser and renderer
// TODO make sure this builds using all major build systems (make, ninja, visual studio msbuild)
// TODO maybe handle xlabels on edges eventually (external label, i.e. the edge goes through the left edge of the
// TODO handle `labelangle` attribute right after the glyph quads have been populated for an edge relative to the top
// left glyph. At this point, I can do trigonometry like with the arrows and rotate the text
// TODO maybe handle color gradients on edges eventually
// label node instead of the center)
// TODO maybe I could add drawing on top xD (i.e. you have a separate surface that you draw into in black and that
// gets rendered last so it overdraws the graph
// TODO XDD I can add animations because I don't have to comply with the dot language spec exactly, just roughly
