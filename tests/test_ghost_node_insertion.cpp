#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <map>
#include <iostream>

using namespace punkt;

TEST(preprocessing, GhostNodeInsertion) {
    // Define a graph with nodes and edges chosen to exercise ghost node insertion:
    // - Edge A -> B: Both nodes have the same rank (0) so ghost insertion (same-rank case) is required.
    // - Edge C -> D: Ranks 1 -> 3 (difference = 2) so ghost node(s) should be inserted.
    // - Edge E -> F: Ranks 3 -> 2 (difference = -1) so no ghost insertion (adjacent ranks).
    // - Edge G -> H: Ranks 3 -> 1 (difference = -2) so ghost node(s) should be inserted.
    const std::string dot_source = R"(
        digraph GhostTest {
            A; B; C; D; E; F; G; H;
            A -> B;
            C -> D;
            E -> F;
            G -> H;
        }
    )";

    // Parse the graph.
    Digraph dg{dot_source};

    // Manually assign ranks to the nodes.
    // (Normally, preprocess() would compute these, but we want to force specific cases.)
    dg.m_nodes.at("A").m_render_attrs.m_rank = 0;
    dg.m_nodes.at("B").m_render_attrs.m_rank = 0; // same as A: diff = 0
    dg.m_nodes.at("C").m_render_attrs.m_rank = 1;
    dg.m_nodes.at("D").m_render_attrs.m_rank = 3; // diff = 2 (3-1)
    dg.m_nodes.at("E").m_render_attrs.m_rank = 3;
    dg.m_nodes.at("F").m_render_attrs.m_rank = 2; // diff = -1 (2-3): adjacent, no ghost insertion
    dg.m_nodes.at("G").m_render_attrs.m_rank = 3;
    dg.m_nodes.at("H").m_render_attrs.m_rank = 1; // diff = -2 (1-3)

    // Initialize m_rank_counts.
    // We need a vector with size at least max(rank)+1. Here max rank = 3.
    dg.m_rank_counts.resize(4, 0);
    // For each existing node, update the count.
    for (auto &[name, node]: dg.m_nodes) {
        // We assume non-ghost nodes (ghosts will be inserted later)
        size_t r = node.m_render_attrs.m_rank;
        dg.m_rank_counts.at(r)++;
    }

    // Insert ghost nodes (this will decompose edges that are not adjacent)
    dg.insertGhostNodes();

    // --- Check ghost insertion for each edge case ---

    // 1. Edge A -> B (same rank: 0 -> 0)
    {
        const Node &node_a = dg.m_nodes.at("A");
        // The original edge should have been decomposed:
        // There should be an outgoing edge from A whose destination is not B (it is a ghost)
        bool found = false;
        for (const Edge &e: node_a.m_outgoing) {
            if (e.m_dest.starts_with("@")) {
                found = true;
                const std::string_view ghost_name = e.m_dest;
                ASSERT_TRUE(dg.m_nodes.contains(ghost_name));
                const Node &ghost_node = dg.m_nodes.at(ghost_name);
                ASSERT_TRUE(ghost_node.m_attrs.find("@type") != ghost_node.m_attrs.end());
                EXPECT_EQ(ghost_node.m_attrs.at("@type"), "ghost");
            }
        }
        EXPECT_TRUE(found) << "Expected ghost node insertion for edge A->B (same rank)";
    }

    // 2. Edge C -> D (ranks 1 -> 3, diff = 2)
    {
        const Node &node_c = dg.m_nodes.at("C");
        bool found = false;
        // Look for an outgoing edge from C that goes to a ghost node rather than directly to D.
        for (const Edge &e: node_c.m_outgoing) {
            if (e.m_dest.starts_with("@")) {
                found = true;
                const std::string_view ghostName = e.m_dest;
                ASSERT_TRUE(dg.m_nodes.contains(ghostName));
                const Node &ghostNode = dg.m_nodes.at(ghostName);
                ASSERT_TRUE(ghostNode.m_attrs.find("@type") != ghostNode.m_attrs.end());
                EXPECT_EQ(ghostNode.m_attrs.at("@type"), "ghost");
            }
        }
        EXPECT_TRUE(found) << "Expected ghost node insertion for edge C->D (rank diff > 1)";
    }

    // 3. Edge E -> F (ranks 3 -> 2, diff = -1) should not be decomposed.
    {
        const Node &node_e = dg.m_nodes.at("E");
        bool found_direct = false;
        for (const Edge &e: node_e.m_outgoing) {
            if (e.m_dest == "F" && e.m_render_attrs.m_is_visible) {
                found_direct = true;
            }
        }
        EXPECT_TRUE(found_direct) << "Edge E->F should remain undisturbed (adjacent ranks)";
    }

    // 4. Edge G -> H (ranks 3 -> 1, diff = -2)
    {
        const Node &node_g = dg.m_nodes.at("G");
        bool found = false;
        for (const Edge &e: node_g.m_outgoing) {
            if (e.m_dest.starts_with("@")) {
                found = true;
                const std::string_view ghostName = e.m_dest;
                ASSERT_TRUE(dg.m_nodes.contains(ghostName));
                const Node &ghost_node = dg.m_nodes.at(ghostName);
                ASSERT_TRUE(ghost_node.m_attrs.find("@type") != ghost_node.m_attrs.end());
                EXPECT_EQ(ghost_node.m_attrs.at("@type"), "ghost");
            }
        }
        EXPECT_TRUE(found) << "Expected ghost node insertion for edge G->H (rank diff < -1)";
    }

    // Optionally, print out all ghost node information for debugging.
    std::string debug_message = "GhostNodeInsertion Ghost Nodes:\n";
    for (const auto &[name, node]: dg.m_nodes) {
        if (node.m_attrs.find("@type") != node.m_attrs.end()) {
            debug_message += "  " + std::string(name) + " at rank " + std::to_string(node.m_render_attrs.m_rank) + "\n";
        }
    }
    std::cout << debug_message;

    SUCCEED();
}
