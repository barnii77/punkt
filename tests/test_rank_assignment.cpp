#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <unordered_map>
#include <string>
#include <iostream>

using namespace punkt;

TEST(preprocessing, RankAssignment) {
    // Define a DAG in DOT language.
    constexpr std::string_view dot_source = R"(
        digraph DAG {
            node [color=black];
            A;
            "B with fancy name";
            C;
            D;
            E;
            A -> "B with fancy name";
            A -> C;
            "B with fancy name" -> D;
            C -> D;
            D -> E;
        }
    )";

    // Parse the DOT source into a Digraph.
    Digraph dg(dot_source);

    // Run the preprocessing step, which computes node ranks.
    render::glyph::GlyphLoader glyph_loader;
    dg.preprocess(glyph_loader);

    // Expected ranks for each node.
    std::unordered_map<std::string_view, size_t> expectedRanks{
        {"A", 0},
        {"B with fancy name", 1},
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
            A -> B -> C -> D -> E;
            C -> A; // Cycle
            // A -> B;
            // B -> C;
            // C -> D;
            // D -> E;
        }
    )";

    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    dg.preprocess(glyph_loader);

    const std::vector<std::vector<std::string_view> > expected_orderings = {
        {"A"},
        {},
        {"C"},
        {"D"},
        {"E"},
    };

    std::cout << "CyclicGraph Rank Visualization:" << std::endl;
    for (size_t rank = 0; const auto &ordering: dg.m_per_rank_orderings) {
        std::cout << "  Rank " << rank << ": ";
        for (const auto &node: ordering) {
            std::cout << node << " ";
        }
        std::cout << std::endl;
        rank++;
    }

    // Check that computed orderings match expected orderings.
    ASSERT_EQ(dg.m_per_rank_orderings.size(), expected_orderings.size())
        << "Rank ordering vector size mismatch.";

    for (size_t rank = 0; rank < expected_orderings.size(); rank++) {
        const auto &expected = expected_orderings[rank];
        const auto &actual = dg.m_per_rank_orderings[rank];
        if (expected.empty()) {
            continue;
        }
        ASSERT_EQ(actual.size(), expected.size()) << "Rank " << rank << " size mismatch.";
        for (size_t i = 0; i < expected.size(); i++) {
            EXPECT_EQ(actual[i], expected[i])
                  << "Mismatch at rank " << rank << ", index " << i
                  << " (expected " << expected[i] << ", got " << actual[i] << ")";
        }
    }
}

TEST(preprocessing, ConstrainedRankAssignment) {
    // Define a DAG in DOT language.
    constexpr std::string_view dot_source = R"(
        digraph DAG {
            X; A; B; C; D; E; F;
            {rank=same; A B C;}
            {rank=min; X;}
            {rank=max; F;}
            X -> B;
            A -> B; A -> C;
            B -> D;
            C -> D;
            D -> E;
            F -> E;
        }
    )";

    // Parse the DOT source into a Digraph.
    Digraph dg(dot_source);

    // Run the preprocessing step, which computes node ranks.
    render::glyph::GlyphLoader glyph_loader;
    dg.populateIngoingNodesVectors();
    dg.computeRanks();
    // dg.preprocess(glyph_loader);

    // Expected ranks for each node.
    std::unordered_map<std::string_view, size_t> expectedRanks{
        {"X", 0},
        {"A", 1},
        {"B", 1},
        {"C", 1},
        {"D", 2},
        {"E", 3},
        {"F", 4},
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
