#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <unordered_map>
#include <string>
#include <iostream>

using namespace dot;

TEST(preprocessing, RankAssignment) {
    // Define a DAG in DOT language.
    constexpr std::string_view dot_source = R"(
        digraph DAG {
            node [color=black];
            A;
            B;
            C;
            D;
            E;
            A -> B;
            A -> C;
            B -> D;
            C -> D;
            D -> E;
        }
    )";

    // Parse the DOT source into a Digraph.
    Digraph dg(dot_source);

    // Run the preprocessing step, which computes node ranks.
    dg.preprocess();

    // Expected ranks for each node.
    std::unordered_map<std::string_view, size_t> expectedRanks{
        {"A", 0},
        {"B", 1},
        {"C", 1},
        {"D", 2},
        {"E", 3}
    };

    // Verify that each node has the expected rank.
    for (const auto &[nodeName, expectedRank]: expectedRanks) {
        ASSERT_TRUE(dg.m_nodes.contains(nodeName)) << "Node " << nodeName << " is missing.";
        const Node &node = dg.m_nodes.at(nodeName);
        EXPECT_EQ(node.m_render_attrs.m_rank, expectedRank)
            << "Node " << nodeName << " has rank " << node.m_render_attrs.m_rank
            << " but expected " << expectedRank;
    }
}

TEST(preprocessing, CyclicGraph) {
    const std::string dot_source = R"(
        digraph CyclicGraph {
            A -> B;
            B -> C;
            C -> A; // Cycle
            C -> D;
            D -> E;
        }
    )";

    Digraph g{dot_source};
    g.preprocess();

    // Group nodes by rank
    std::map<size_t, std::vector<std::string>> rank_map;
    for (const auto &[name, node]: g.m_nodes) {
        rank_map[node.m_render_attrs.m_rank].emplace_back(name);
    }

    // Generate visualization
    std::string message = "CyclicGraph Rank Visualization:\n";
    for (const auto &[rank, nodes]: rank_map) {
        message += "  Rank " + std::to_string(rank) + ": ";
        for (const auto &name : nodes) {
            message += name + " ";
        }
        message += '\n';
    }

    std::cout << message;
    SUCCEED();
}
