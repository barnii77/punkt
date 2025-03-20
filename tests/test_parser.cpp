#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <algorithm>

using namespace punkt;

TEST(parser, SimpleTest) {
    // This DOT source tests:
    // - Setting default attributes for nodes and edges.
    // - An explicit node "A" (no extra attributes) should inherit node defaults.
    // - An explicit node "B" overriding one default attribute.
    // - An edge chain "A -> B -> C -> D" with explicit edge attributes that override part of the edge defaults.
    // - An explicit node "E" with its own attribute that overrides the default.
    // - An isolated edge "D -> E" with no explicit attributes (so only the edge defaults apply).
    std::string dot_source = R"(
        digraph MyGraph {
            // Set default attributes for nodes and edges.
            node [color=red, shape=circle];
            edge [style=dotted, weight=1];

            // Explicit node declarations.
            A;
            B [shape=box];

            // An edge chain: create edges A->B, B->C, and C->D.
            // Explicit attributes on the chain: label and weight.
            // These will be merged with the default edge attributes.
            A -> B -> C -> D [label=chain, weight=2];

            // An explicit node with its own attribute; should inherit the default "shape".
            E [color=blue];

            // An isolated edge; no explicit attributes so defaults apply.
            D -> E;
        }
    )";

    // Parse the graph.
    Digraph dg;
    try {
        dg = Digraph(dot_source);
        dg.populateIngoingNodesVectors();
    } catch (const std::exception &e) {
        FAIL() << "Parsing failed with exception: " << e.what();
    }

    // === Graph Basics ===
    // Check that the graph name was correctly set.
    EXPECT_EQ(dg.m_name, "MyGraph");

    // Check that the default node attributes were captured.
    ASSERT_TRUE(dg.m_default_node_attrs.find("color") != dg.m_default_node_attrs.end());
    ASSERT_TRUE(dg.m_default_node_attrs.find("shape") != dg.m_default_node_attrs.end());
    EXPECT_EQ(dg.m_default_node_attrs.at("color"), "red");
    EXPECT_EQ(dg.m_default_node_attrs.at("shape"), "circle");

    // Check that the default edge attributes were captured.
    ASSERT_TRUE(dg.m_default_edge_attrs.find("style") != dg.m_default_edge_attrs.end());
    ASSERT_TRUE(dg.m_default_edge_attrs.find("weight") != dg.m_default_edge_attrs.end());
    EXPECT_EQ(dg.m_default_edge_attrs.at("style"), "dotted");
    EXPECT_EQ(dg.m_default_edge_attrs.at("weight"), "1");

    // === Node Declarations ===

    // Node A: declared explicitly without attributes, so it should use the default node attrs.
    ASSERT_TRUE(dg.m_nodes.contains("A"));
    const Node& nodeA = dg.m_nodes.at("A");
    EXPECT_EQ(nodeA.m_name, "A");
    EXPECT_EQ(nodeA.m_attrs.at("color"), "red");
    EXPECT_EQ(nodeA.m_attrs.at("shape"), "circle");

    // Node B: declared with an explicit shape override.
    ASSERT_TRUE(dg.m_nodes.contains("B"));
    const Node& nodeB = dg.m_nodes.at("B");
    EXPECT_EQ(nodeB.m_name, "B");
    // Explicit "shape" should override the default, but missing keys are filled in.
    EXPECT_EQ(nodeB.m_attrs.at("color"), "red");
    EXPECT_EQ(nodeB.m_attrs.at("shape"), "box");

    // Nodes C and D: created implicitly in the edge chain.
    // They are created with an empty attribute map.
    ASSERT_TRUE(dg.m_nodes.contains("C"));
    const Node& nodeC = dg.m_nodes.at("C");
    EXPECT_EQ(nodeC.m_name, "C");
    EXPECT_EQ(nodeC.m_attrs, dg.m_default_node_attrs);

    ASSERT_TRUE(dg.m_nodes.contains("D"));
    const Node& nodeD = dg.m_nodes.at("D");
    EXPECT_EQ(nodeD.m_name, "D");
    EXPECT_EQ(nodeD.m_attrs, dg.m_default_node_attrs);

    // Node E: declared explicitly with an override.
    ASSERT_TRUE(dg.m_nodes.contains("E"));
    const Node& nodeE = dg.m_nodes.at("E");
    EXPECT_EQ(nodeE.m_name, "E");
    // "color" is overridden to blue; missing "shape" comes from defaults.
    EXPECT_EQ(nodeE.m_attrs.at("color"), "blue");
    EXPECT_EQ(nodeE.m_attrs.at("shape"), "circle");

    // === Edge Declarations ===

    // Edge chain "A -> B -> C -> D [label=chain, weight=2]" should create:
    //  - An edge from A to B,
    //  - An edge from B to C,
    //  - An edge from C to D.
    // Each edge's attributes are formed by merging the explicit attributes
    // {label=chain, weight=2} with the default edge attrs {style=dotted, weight=1}.
    // Because merge inserts default values only if the key is absent,
    // explicit "weight=2" remains and "style=dotted" is added.
    const std::unordered_map<std::string, std::string> expectedEdgeAttrsChain{
        {"label", "chain"}, {"weight", "2"}, {"style", "dotted"}
    };

    // For node A, expect one outgoing edge: A -> B.
    ASSERT_EQ(nodeA.m_outgoing.size(), 1);
    const Edge& edgeAB = nodeA.m_outgoing.front();
    EXPECT_EQ(edgeAB.m_source, "A");
    EXPECT_EQ(edgeAB.m_dest, "B");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(edgeAB.m_attrs.at(key), val);
    }

    // For node B, expect one outgoing edge: B -> C.
    ASSERT_EQ(nodeB.m_outgoing.size(), 1);
    const Edge& edgeBC = nodeB.m_outgoing.front();
    EXPECT_EQ(edgeBC.m_source, "B");
    EXPECT_EQ(edgeBC.m_dest, "C");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(edgeBC.m_attrs.at(key), val);
    }

    // For node C, expect one outgoing edge: C -> D.
    ASSERT_EQ(nodeC.m_outgoing.size(), 1);
    const Edge& edgeCD = nodeC.m_outgoing.front();
    EXPECT_EQ(edgeCD.m_source, "C");
    EXPECT_EQ(edgeCD.m_dest, "D");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(edgeCD.m_attrs.at(key), val);
    }

    // The isolated edge "D -> E" should have no explicit attributes,
    // so its attributes should be exactly the default edge attributes.
    ASSERT_EQ(nodeD.m_outgoing.size(), 1);
    const Edge& edgeDE = nodeD.m_outgoing.front();
    EXPECT_EQ(edgeDE.m_source, "D");
    EXPECT_EQ(edgeDE.m_dest, "E");
    EXPECT_EQ(edgeDE.m_attrs.at("style"), "dotted");
    EXPECT_EQ(edgeDE.m_attrs.at("weight"), "1");

    // === Ingoing Edge Checks ===

    // For node B, its ingoing edge should be A -> B.
    ASSERT_EQ(nodeB.m_ingoing.size(), 1);
    const Edge &ingoingB = nodeB.m_ingoing.front();
    EXPECT_EQ(ingoingB.m_source, "A");
    EXPECT_EQ(ingoingB.m_dest, "B");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(ingoingB.m_attrs.at(key), val);
    }

    // For node C, its ingoing edge should be B -> C.
    ASSERT_EQ(nodeC.m_ingoing.size(), 1);
    const Edge &ingoingC = nodeC.m_ingoing.front();
    EXPECT_EQ(ingoingC.m_source, "B");
    EXPECT_EQ(ingoingC.m_dest, "C");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(ingoingC.m_attrs.at(key), val);
    }

    // For node D, its ingoing edge should be C -> D.
    ASSERT_EQ(nodeD.m_ingoing.size(), 1);
    const Edge &ingoingD = nodeD.m_ingoing.front();
    EXPECT_EQ(ingoingD.m_source, "C");
    EXPECT_EQ(ingoingD.m_dest, "D");
    for (const auto& [key, val] : expectedEdgeAttrsChain) {
        EXPECT_EQ(ingoingD.m_attrs.at(key), val);
    }

    // For node E, its ingoing edge should be D -> E.
    ASSERT_EQ(nodeE.m_ingoing.size(), 1);
    const Edge &ingoingE = nodeE.m_ingoing.front();
    EXPECT_EQ(ingoingE.m_source, "D");
    EXPECT_EQ(ingoingE.m_dest, "E");
    EXPECT_EQ(ingoingE.m_attrs.at("style"), "dotted");
    EXPECT_EQ(ingoingE.m_attrs.at("weight"), "1");
}

TEST(parser, ComplexCyclicTest) {
    // This DOT source exercises:
    // - Two rounds of default node and edge attribute settings (with merging).
    // - Explicit node declarations (with and without attribute overrides).
    // - Implicit node creation in edge chains.
    // - Two edge chains with explicit edge attributes that merge with defaults.
    // - Isolated edges using updated default edge attributes.
    // - Additional cycle edges:
    //     - V -> Y to create a cycle from V back to Y.
    //     - T -> X and X -> T to form a cycle between T and X.
    //     - Z -> Z as a self-loop.
    std::string dot_source = R"(
        digraph ComplexGraph {
            // Initial default attributes.
            node [color=green, shape=ellipse, fontname="Arial"];
            edge [style=solid, weight=1];

            // Explicit node declarations using first defaults.
            X;
            Y [color=blue];
            Z;

            // Edge Chain 1: X -> Y -> Z with explicit edge attributes.
            // Explicit attributes override the initial defaults.
            X -> Y -> Z [style=dashed, label="Chain1", weight=2];

            // Update default node attributes (merging with the old defaults).
            // New defaults become: {color=purple, fontsize=12} plus inherited keys (shape and fontname).
            node [color=purple, fontsize=12];

            // New node declarations after default update.
            W;
            V [shape=box, fontsize=16];  // Overrides shape and fontsize.

            // Edge Chain 2: Y -> W -> V -> U with explicit edge attributes.
            // U is created implicitly.
            Y -> W -> V -> U [label="Chain2", weight=3];

            // Update default edge attributes.
            // New defaults: {style=bold} (and weight remains 1 if not overridden).
            edge [style=bold];

            // Isolated edge using the updated default edge attributes.
            U -> X [label="BackEdge"];

            // An isolated node with explicit overrides.
            T [color=orange, fontname="Courier"];

            // An isolated edge with no explicit attributes; it inherits the updated defaults.
            T -> V;

            // --- Additional cycle edges ---
            // Create a cycle from V back to Y.
            V -> Y [label="CycleEdge1"];
            // Create a cycle between T and X.
            T -> X [label="CycleEdge2"];
            X -> T [label="CycleEdge3"];
            // Create a self-loop on Z.
            Z -> Z [label="SelfLoop"];
        }
    )";

    // Parse the graph.
    Digraph dg;
    try {
        dg = Digraph(dot_source);
        dg.populateIngoingNodesVectors();
    } catch (const std::exception &e) {
        FAIL() << "Parsing failed with exception: " << e.what();
    }

    // === Graph Basics ===
    EXPECT_EQ(dg.m_name, "ComplexGraph");

    // Final default node attributes:
    // After "node [color=purple, fontsize=12]", merging with the initial defaults
    // {color=green, shape=ellipse, fontname="Arial"} yields:
    // {color=purple, fontsize=12, shape=ellipse, fontname="Arial"}.
    std::unordered_map<std::string_view, std::string_view> expectedFinalNodeDefaults{
        {"color", "purple"}, {"fontsize", "12"}, {"shape", "ellipse"}, {"fontname", "Arial"}
    };
    EXPECT_EQ(dg.m_default_node_attrs, expectedFinalNodeDefaults);

    // Final default edge attributes:
    // After "edge [style=bold]", merging with the initial edge defaults {style=solid, weight=1}
    // yields: {style=bold, weight=1}.
    std::unordered_map<std::string_view, std::string_view> expectedFinalEdgeDefaults{
        {"style", "bold"}, {"weight", "1"}
    };
    EXPECT_EQ(dg.m_default_edge_attrs, expectedFinalEdgeDefaults);

    // === Node Declarations ===

    // Node X: Declared in the first block; attributes from initial defaults.
    ASSERT_TRUE(dg.m_nodes.contains("X"));
    const Node& nodeX = dg.m_nodes.at("X");
    std::unordered_map<std::string_view, std::string_view> expectedX{
        {"color", "green"}, {"shape", "ellipse"}, {"fontname", "Arial"}
    };
    EXPECT_EQ(nodeX.m_attrs, expectedX);

    // Node Y: Declared in the first block with override [color=blue].
    ASSERT_TRUE(dg.m_nodes.contains("Y"));
    const Node& nodeY = dg.m_nodes.at("Y");
    std::unordered_map<std::string_view, std::string_view> expectedY{
        {"color", "blue"}, {"shape", "ellipse"}, {"fontname", "Arial"}
    };
    EXPECT_EQ(nodeY.m_attrs, expectedY);

    // Node Z: Declared explicitly; from initial defaults.
    ASSERT_TRUE(dg.m_nodes.contains("Z"));
    const Node& nodeZ = dg.m_nodes.at("Z");
    EXPECT_EQ(nodeZ.m_attrs, expectedX);

    // Node W: Declared after the node default update; should have the new defaults.
    ASSERT_TRUE(dg.m_nodes.contains("W"));
    const Node& nodeW = dg.m_nodes.at("W");
    EXPECT_EQ(nodeW.m_attrs, expectedFinalNodeDefaults);

    // Node V: Declared with explicit [shape=box, fontsize=16] after update;
    // expected: explicit keys override, merged with inherited {color=purple, fontname="Arial"}.
    ASSERT_TRUE(dg.m_nodes.contains("V"));
    const Node& nodeV = dg.m_nodes.at("V");
    std::unordered_map<std::string_view, std::string_view> expectedV{
        {"shape", "box"}, {"fontsize", "16"}, {"color", "purple"}, {"fontname", "Arial"}
    };
    EXPECT_EQ(nodeV.m_attrs, expectedV);

    // Node U: Implicitly created in Edge Chain 2; no attributes.
    ASSERT_TRUE(dg.m_nodes.contains("U"));
    const Node& nodeU = dg.m_nodes.at("U");
    EXPECT_EQ(nodeU.m_attrs, dg.m_default_node_attrs);

    // Node T: Declared explicitly with overrides; merge with updated defaults.
    ASSERT_TRUE(dg.m_nodes.contains("T"));
    const Node& nodeT = dg.m_nodes.at("T");
    std::unordered_map<std::string_view, std::string_view> expectedT{
        {"color", "orange"}, {"fontname", "Courier"}, {"fontsize", "12"}, {"shape", "ellipse"}
    };
    EXPECT_EQ(nodeT.m_attrs, expectedT);

    // === Edge Declarations ===

    // -- Edge Chain 1: "X -> Y -> Z [style=dashed, label="Chain1", weight=2]"
    std::unordered_map<std::string_view, std::string_view> expectedChain1{
        {"style", "dashed"}, {"label", "Chain1"}, {"weight", "2"}
    };

    // For node X, expect an outgoing edge: X -> Y.
    // Note: Later we add a cycle edge from X -> T.
    ASSERT_EQ(nodeX.m_outgoing.size(), 2);
    auto edgeXY_it = std::find_if(nodeX.m_outgoing.begin(), nodeX.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "Y"; });
    ASSERT_NE(edgeXY_it, nodeX.m_outgoing.end());
    for (const auto & [key, val] : expectedChain1) {
        EXPECT_EQ(edgeXY_it->m_attrs.at(key), val);
    }

    // -- Edge Chain 2: "Y -> W -> V -> U [label="Chain2", weight=3]"
    std::unordered_map<std::string_view, std::string_view> expectedChain2{
        {"label", "Chain2"}, {"weight", "3"}, {"style", "solid"}
    };

    // For node Y, expect an outgoing edge: Y -> W.
    ASSERT_EQ(nodeY.m_outgoing.size(), 2);
    auto edgeYW_it = std::find_if(nodeY.m_outgoing.begin(), nodeY.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "W"; });
    ASSERT_NE(edgeYW_it, nodeY.m_outgoing.end());
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(edgeYW_it->m_attrs.at(key), val);
    }

    // For node W, expect one outgoing edge: W -> V.
    ASSERT_EQ(nodeW.m_outgoing.size(), 1);
    const Edge& edgeWV = nodeW.m_outgoing.front();
    EXPECT_EQ(edgeWV.m_source, "W");
    EXPECT_EQ(edgeWV.m_dest, "V");
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(edgeWV.m_attrs.at(key), val);
    }

    // For node V, expect an outgoing edge: V -> U from Chain 2.
    // Also, V has a cycle edge later.
    ASSERT_EQ(nodeV.m_outgoing.size(), 2);
    auto edgeVU_it = std::find_if(nodeV.m_outgoing.begin(), nodeV.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "U"; });
    ASSERT_NE(edgeVU_it, nodeV.m_outgoing.end());
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(edgeVU_it->m_attrs.at(key), val);
    }

    // -- Isolated Edge 1: "U -> X [label="BackEdge"]"
    std::unordered_map<std::string_view, std::string_view> expectedIsoEdge1{
        {"label", "BackEdge"}, {"style", "bold"}, {"weight", "1"}
    };
    ASSERT_EQ(nodeU.m_outgoing.size(), 1);
    const Edge& edgeUX = nodeU.m_outgoing.front();
    EXPECT_EQ(edgeUX.m_source, "U");
    EXPECT_EQ(edgeUX.m_dest, "X");
    for (const auto & [key, val] : expectedIsoEdge1) {
        EXPECT_EQ(edgeUX.m_attrs.at(key), val);
    }

    // -- Isolated Edge 2: "T -> V"
    std::unordered_map<std::string_view, std::string_view> expectedIsoEdge2{
        {"style", "bold"}, {"weight", "1"}
    };
    ASSERT_EQ(nodeT.m_outgoing.size(), 2);  // T has two outgoing edges: T->V and a cycle edge T->X.
    auto edgeTV_it = std::find_if(nodeT.m_outgoing.begin(), nodeT.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "V"; });
    ASSERT_NE(edgeTV_it, nodeT.m_outgoing.end());
    for (const auto & [key, val] : expectedIsoEdge2) {
        EXPECT_EQ(edgeTV_it->m_attrs.at(key), val);
    }

    // --- Cycle Edges ---

    // Cycle Edge from V -> Y:
    std::unordered_map<std::string_view, std::string_view> expectedCycleEdge{
        {"label", "CycleEdge1"}, {"style", "bold"}, {"weight", "1"}
    };
    auto edgeVY_it = std::find_if(nodeV.m_outgoing.begin(), nodeV.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "Y"; });
    ASSERT_NE(edgeVY_it, nodeV.m_outgoing.end());
    for (const auto & [key, val] : expectedCycleEdge) {
        EXPECT_EQ(edgeVY_it->m_attrs.at(key), val);
    }

    // Cycle Edge from T -> X:
    std::unordered_map<std::string_view, std::string_view> expectedCycleEdge2{
        {"label", "CycleEdge2"}, {"style", "bold"}, {"weight", "1"}
    };
    auto edgeTX_it = std::find_if(nodeT.m_outgoing.begin(), nodeT.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "X"; });
    ASSERT_NE(edgeTX_it, nodeT.m_outgoing.end());
    for (const auto & [key, val] : expectedCycleEdge2) {
        EXPECT_EQ(edgeTX_it->m_attrs.at(key), val);
    }

    // Cycle Edge from X -> T:
    std::unordered_map<std::string_view, std::string_view> expectedCycleEdge3{
        {"label", "CycleEdge3"}, {"style", "bold"}, {"weight", "1"}
    };
    auto edgeXT_it = std::find_if(nodeX.m_outgoing.begin(), nodeX.m_outgoing.end(),
                                  [](const Edge &e){ return e.m_dest == "T"; });
    ASSERT_NE(edgeXT_it, nodeX.m_outgoing.end());
    for (const auto & [key, val] : expectedCycleEdge3) {
        EXPECT_EQ(edgeXT_it->m_attrs.at(key), val);
    }

    // Self-loop on Z: "Z -> Z [label="SelfLoop"]"
    std::unordered_map<std::string_view, std::string_view> expectedSelfLoop{
        {"label", "SelfLoop"}, {"style", "bold"}, {"weight", "1"}
    };
    ASSERT_EQ(nodeZ.m_outgoing.size(), 1);
    const Edge& edgeZZ = nodeZ.m_outgoing.front();
    EXPECT_EQ(edgeZZ.m_source, "Z");
    EXPECT_EQ(edgeZZ.m_dest, "Z");
    for (const auto & [key, val] : expectedSelfLoop) {
        EXPECT_EQ(edgeZZ.m_attrs.at(key), val);
    }

    // === Ingoing Edge Checks for Complex Graph ===

    // For node X, ingoing edges should be from U->X (BackEdge) and T->X (CycleEdge2).
    ASSERT_EQ(nodeX.m_ingoing.size(), 2);
    auto incX_back = std::find_if(nodeX.m_ingoing.begin(), nodeX.m_ingoing.end(),
                                  [](const Edge &e){ return e.m_source == "U" && e.m_dest == "X"; });
    ASSERT_NE(incX_back, nodeX.m_ingoing.end());
    for (const auto & [key, val] : expectedIsoEdge1) {
        EXPECT_EQ(incX_back->get().m_attrs.at(key), val);
    }
    auto incX_cycle = std::find_if(nodeX.m_ingoing.begin(), nodeX.m_ingoing.end(),
                                   [](const Edge &e){ return e.m_source == "T" && e.m_dest == "X"; });
    ASSERT_NE(incX_cycle, nodeX.m_ingoing.end());
    for (const auto & [key, val] : expectedCycleEdge2) {
        EXPECT_EQ(incX_cycle->get().m_attrs.at(key), val);
    }

    // For node Y, ingoing edges should be from X->Y (Chain1) and V->Y (CycleEdge1).
    ASSERT_EQ(nodeY.m_ingoing.size(), 2);
    auto incY_chain = std::find_if(nodeY.m_ingoing.begin(), nodeY.m_ingoing.end(),
                                   [](const Edge &e){ return e.m_source == "X" && e.m_dest == "Y"; });
    ASSERT_NE(incY_chain, nodeY.m_ingoing.end());
    for (const auto & [key, val] : expectedChain1) {
        EXPECT_EQ(incY_chain->get().m_attrs.at(key), val);
    }
    auto incY_cycle = std::find_if(nodeY.m_ingoing.begin(), nodeY.m_ingoing.end(),
                                   [](const Edge &e){ return e.m_source == "V" && e.m_dest == "Y"; });
    ASSERT_NE(incY_cycle, nodeY.m_ingoing.end());
    for (const auto & [key, val] : expectedCycleEdge) {
        EXPECT_EQ(incY_cycle->get().m_attrs.at(key), val);
    }

    // For node Z, ingoing edges should be from Y->Z (Chain1) and the self-loop Z->Z.
    ASSERT_EQ(nodeZ.m_ingoing.size(), 2);
    auto incZ_chain = std::find_if(nodeZ.m_ingoing.begin(), nodeZ.m_ingoing.end(),
                                   [](const Edge &e){ return e.m_source == "Y" && e.m_dest == "Z"; });
    ASSERT_NE(incZ_chain, nodeZ.m_ingoing.end());
    for (const auto & [key, val] : expectedChain1) {
        EXPECT_EQ(incZ_chain->get().m_attrs.at(key), val);
    }
    auto incZ_self = std::find_if(nodeZ.m_ingoing.begin(), nodeZ.m_ingoing.end(),
                                  [](const Edge &e){ return e.m_source == "Z" && e.m_dest == "Z"; });
    ASSERT_NE(incZ_self, nodeZ.m_ingoing.end());
    for (const auto & [key, val] : expectedSelfLoop) {
        EXPECT_EQ(incZ_self->get().m_attrs.at(key), val);
    }

    // For node W, ingoing edge should be from Y->W (Chain2).
    ASSERT_EQ(nodeW.m_ingoing.size(), 1);
    const Edge &incW = nodeW.m_ingoing.front();
    EXPECT_EQ(incW.m_source, "Y");
    EXPECT_EQ(incW.m_dest, "W");
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(incW.m_attrs.at(key), val);
    }

    // For node V, ingoing edges should be from W->V (Chain2) and T->V (Isolated Edge 2).
    ASSERT_EQ(nodeV.m_ingoing.size(), 2);
    auto incV_chain = std::find_if(nodeV.m_ingoing.begin(), nodeV.m_ingoing.end(),
                                   [](const Edge &e){ return e.m_source == "W" && e.m_dest == "V"; });
    ASSERT_NE(incV_chain, nodeV.m_ingoing.end());
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(incV_chain->get().m_attrs.at(key), val);
    }
    auto incV_iso = std::find_if(nodeV.m_ingoing.begin(), nodeV.m_ingoing.end(),
                                 [](const Edge &e){ return e.m_source == "T" && e.m_dest == "V"; });
    ASSERT_NE(incV_iso, nodeV.m_ingoing.end());
    for (const auto & [key, val] : expectedIsoEdge2) {
        EXPECT_EQ(incV_iso->get().m_attrs.at(key), val);
    }

    // For node U, ingoing edge should be from V->U (Chain2).
    ASSERT_EQ(nodeU.m_ingoing.size(), 1);
    const Edge &incU = nodeU.m_ingoing.front();
    EXPECT_EQ(incU.m_source, "V");
    EXPECT_EQ(incU.m_dest, "U");
    for (const auto & [key, val] : expectedChain2) {
        EXPECT_EQ(incU.m_attrs.at(key), val);
    }

    // For node T, ingoing edge should be from X->T (CycleEdge3).
    ASSERT_EQ(nodeT.m_ingoing.size(), 1);
    const Edge &incT = nodeT.m_ingoing.front();
    EXPECT_EQ(incT.m_source, "X");
    EXPECT_EQ(incT.m_dest, "T");
    for (const auto & [key, val] : expectedCycleEdge3) {
        EXPECT_EQ(incT.m_attrs.at(key), val);
    }
}
