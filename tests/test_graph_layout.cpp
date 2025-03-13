#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <iostream>
#include <ranges>

using namespace punkt;

static void emitRect(std::stringstream &ss, const size_t left, const size_t top, const size_t right,
                     const size_t bottom) {
    ss << "(" << left << ", " << top << ", " << right << ", " << bottom << ")\n";
}

TEST(preprocessing, GraphLayout) {
    const std::string dot_source = R"(
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

    // Parse the graph. Let preprocess() compute node ranks and also perform ghost insertion.
    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    dg.preprocess(glyph_loader);

    std::stringstream ss;

    // Output graph dimensions and properties
    ss << "Graph(" << dg.m_render_attrs.m_graph_width << ", " << dg.m_render_attrs.m_graph_height << ")\n";
    ss << "RankSep=" << dg.m_render_attrs.m_rank_sep << " NodeSep=" << dg.m_render_attrs.m_node_sep << "\n";

    // Output rank information
    for (size_t i = 0; i < dg.m_render_attrs.m_rank_render_attrs.size(); i++) {
        const auto &rank = dg.m_render_attrs.m_rank_render_attrs[i];
        ss << "Rank " << i << "(" << rank.m_rank_x << ", " << rank.m_rank_y << ", "
                << rank.m_rank_width << ", " << rank.m_rank_height << ")\n";
    }

    // Output nodes with their absolute positions
    for (const Node &node: std::views::values(dg.m_nodes)) {
        // Skip ghost nodes for the visualization
        if (node.m_render_attrs.m_is_ghost) {
            continue;
        }

        ss << "Node " << node.m_name;
        // Output the node's position and dimensions
        emitRect(ss, node.m_render_attrs.m_x, node.m_render_attrs.m_y,
                 node.m_render_attrs.m_x + node.m_render_attrs.m_width,
                 node.m_render_attrs.m_y + node.m_render_attrs.m_height);

        // Output the glyph quads for this node
        for (const GlyphQuad &gq: node.m_render_attrs.m_quads) {
            ss << "Quad";
            // Convert quad to absolute coordinates
            emitRect(ss,
                     node.m_render_attrs.m_x + gq.left,
                     node.m_render_attrs.m_y + gq.top,
                     node.m_render_attrs.m_x + gq.right,
                     node.m_render_attrs.m_y + gq.bottom);
        }
    }

    // Output edges (optional, for fuller visualization)
    for (const Node &node: std::views::values(dg.m_nodes)) {
        for (const Edge &edge: node.m_outgoing) {
            ss << "Edge " << edge.m_source << " -> " << edge.m_dest << "\n";
        }
    }

    std::string s = ss.str();

    constexpr std::string_view expected = R"(Graph(489, 380)
RankSep=75 NodeSep=50
Rank 0(31, 0, 427, 39)
Rank 1(46, 114, 396, 55)
Rank 2(0, 244, 489, 39)
Rank 3(158, 358, 173, 22)
Node A(31, 0, 188, 39)
Quad(60, 4, 74, 18)
Quad(74, 4, 88, 18)
Quad(88, 4, 102, 18)
Quad(102, 4, 116, 18)
Quad(116, 4, 130, 18)
Quad(130, 4, 144, 18)
Quad(144, 4, 158, 18)
Quad(39, 20, 53, 34)
Quad(53, 20, 67, 34)
Quad(67, 20, 81, 34)
Quad(81, 20, 95, 34)
Quad(95, 20, 109, 34)
Quad(109, 20, 123, 34)
Quad(123, 20, 137, 34)
Quad(137, 20, 151, 34)
Quad(151, 20, 165, 34)
Quad(165, 20, 179, 34)
Node B(238, 0, 458, 39)
Quad(299, 4, 313, 18)
Quad(313, 4, 327, 18)
Quad(327, 4, 341, 18)
Quad(341, 4, 355, 18)
Quad(355, 4, 369, 18)
Quad(369, 4, 383, 18)
Quad(383, 4, 397, 18)
Quad(250, 20, 264, 34)
Quad(264, 20, 278, 34)
Quad(278, 20, 292, 34)
Quad(292, 20, 306, 34)
Quad(306, 20, 320, 34)
Quad(320, 20, 334, 34)
Quad(334, 20, 348, 34)
Quad(348, 20, 362, 34)
Quad(362, 20, 376, 34)
Quad(376, 20, 390, 34)
Quad(390, 20, 404, 34)
Quad(404, 20, 418, 34)
Quad(418, 20, 432, 34)
Quad(432, 20, 446, 34)
Node C(46, 114, 219, 169)
Quad(76, 118, 90, 132)
Quad(90, 118, 104, 132)
Quad(104, 118, 118, 132)
Quad(118, 118, 132, 132)
Quad(132, 118, 146, 132)
Quad(146, 118, 160, 132)
Quad(160, 118, 174, 132)
Quad(174, 118, 188, 132)
Quad(83, 134, 97, 148)
Quad(97, 134, 111, 148)
Quad(111, 134, 125, 148)
Quad(125, 134, 139, 148)
Quad(139, 134, 153, 148)
Quad(153, 134, 167, 148)
Quad(167, 134, 181, 148)
Quad(55, 150, 69, 164)
Quad(69, 150, 83, 164)
Quad(83, 150, 97, 164)
Quad(97, 150, 111, 164)
Quad(111, 150, 125, 164)
Quad(125, 150, 139, 164)
Quad(139, 150, 153, 164)
Quad(153, 150, 167, 164)
Quad(167, 150, 181, 164)
Quad(181, 150, 195, 164)
Quad(195, 150, 209, 164)
Node D(269, 122, 442, 161)
Quad(320, 126, 334, 140)
Quad(334, 126, 348, 140)
Quad(348, 126, 362, 140)
Quad(362, 126, 376, 140)
Quad(376, 126, 390, 140)
Quad(278, 142, 292, 156)
Quad(292, 142, 306, 156)
Quad(306, 142, 320, 156)
Quad(320, 142, 334, 156)
Quad(334, 142, 348, 156)
Quad(348, 142, 362, 156)
Quad(362, 142, 376, 156)
Quad(376, 142, 390, 156)
Quad(390, 142, 404, 156)
Quad(404, 142, 418, 156)
Quad(418, 142, 432, 156)
Node E(0, 252, 235, 274)
Quad(12, 256, 26, 270)
Quad(26, 256, 40, 270)
Quad(40, 256, 54, 270)
Quad(54, 256, 68, 270)
Quad(68, 256, 82, 270)
Quad(82, 256, 96, 270)
Quad(96, 256, 110, 270)
Quad(110, 256, 124, 270)
Quad(124, 256, 138, 270)
Quad(138, 256, 152, 270)
Quad(152, 256, 166, 270)
Quad(166, 256, 180, 270)
Quad(180, 256, 194, 270)
Quad(194, 256, 208, 270)
Quad(208, 256, 222, 270)
Node F(285, 244, 489, 283)
Quad(338, 248, 352, 262)
Quad(352, 248, 366, 262)
Quad(366, 248, 380, 262)
Quad(380, 248, 394, 262)
Quad(394, 248, 408, 262)
Quad(408, 248, 422, 262)
Quad(422, 248, 436, 262)
Quad(296, 264, 310, 278)
Quad(310, 264, 324, 278)
Quad(324, 264, 338, 278)
Quad(338, 264, 352, 278)
Quad(352, 264, 366, 278)
Quad(366, 264, 380, 278)
Quad(380, 264, 394, 278)
Quad(394, 264, 408, 278)
Quad(408, 264, 422, 278)
Quad(422, 264, 436, 278)
Quad(436, 264, 450, 278)
Quad(450, 264, 464, 278)
Quad(464, 264, 478, 278)
Node G(158, 358, 331, 380)
Quad(167, 362, 181, 376)
Quad(181, 362, 195, 376)
Quad(195, 362, 209, 376)
Quad(209, 362, 223, 376)
Quad(223, 362, 237, 376)
Quad(237, 362, 251, 376)
Quad(251, 362, 265, 376)
Quad(265, 362, 279, 376)
Quad(279, 362, 293, 376)
Quad(293, 362, 307, 376)
Quad(307, 362, 321, 376)
Edge A -> C
Edge A -> D
Edge B -> C
Edge B -> D
Edge C -> E
Edge D -> F
Edge E -> G
Edge F -> G
)";

    ASSERT_EQ(expected, s);
}

TEST(preprocessing, GraphLayoutWithEdges) {
    const std::string dot_source = R"(
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

    // Parse the graph. Let preprocess() compute node ranks and perform ghost insertion.
    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    dg.preprocess(glyph_loader);

    std::stringstream ss;

    // Output graph dimensions and properties
    ss << "Graph(" << dg.m_render_attrs.m_graph_width << ", " << dg.m_render_attrs.m_graph_height << ")\n";
    ss << "RankSep=" << dg.m_render_attrs.m_rank_sep << " NodeSep=" << dg.m_render_attrs.m_node_sep << "\n";

    // Output rank information
    for (size_t i = 0; i < dg.m_render_attrs.m_rank_render_attrs.size(); i++) {
        const auto &rank = dg.m_render_attrs.m_rank_render_attrs[i];
        ss << "Rank " << i << "(" << rank.m_rank_x << ", " << rank.m_rank_y << ", "
                << rank.m_rank_width << ", " << rank.m_rank_height << ")\n";
    }

    // Output nodes with their absolute positions (skip ghost nodes)
    for (const Node &node: std::views::values(dg.m_nodes)) {
        if (node.m_render_attrs.m_is_ghost)
            continue;
        ss << "Node " << node.m_name;
        emitRect(ss, node.m_render_attrs.m_x, node.m_render_attrs.m_y,
                 node.m_render_attrs.m_x + node.m_render_attrs.m_width,
                 node.m_render_attrs.m_y + node.m_render_attrs.m_height);
        for (const GlyphQuad &gq: node.m_render_attrs.m_quads) {
            ss << "Quad";
            emitRect(ss,
                     node.m_render_attrs.m_x + gq.left,
                     node.m_render_attrs.m_y + gq.top,
                     node.m_render_attrs.m_x + gq.right,
                     node.m_render_attrs.m_y + gq.bottom);
        }
    }

    for (const Node &node : std::views::values(dg.m_nodes)) {
        for (const Edge &edge : node.m_outgoing) {
            ss << "Edge(";
            const auto &trajectory = edge.m_render_attrs.m_trajectory;
            for (size_t i = 0; i < trajectory.size(); i++) {
                const auto &pt = trajectory[i];
                ss << "(" << pt.x << ", " << pt.y << ")";
                if (i + 1 < trajectory.size()) {
                    ss << ", ";
                }
            }
            ss << ")\n";
        }
    }

    std::string s = ss.str();

    constexpr std::string_view expected = R"(Graph(489, 380)
RankSep=75 NodeSep=50
Rank 0(31, 0, 427, 39)
Rank 1(46, 114, 396, 55)
Rank 2(0, 244, 489, 39)
Rank 3(158, 358, 173, 22)
Node A(31, 0, 188, 39)
Quad(60, 4, 74, 18)
Quad(74, 4, 88, 18)
Quad(88, 4, 102, 18)
Quad(102, 4, 116, 18)
Quad(116, 4, 130, 18)
Quad(130, 4, 144, 18)
Quad(144, 4, 158, 18)
Quad(39, 20, 53, 34)
Quad(53, 20, 67, 34)
Quad(67, 20, 81, 34)
Quad(81, 20, 95, 34)
Quad(95, 20, 109, 34)
Quad(109, 20, 123, 34)
Quad(123, 20, 137, 34)
Quad(137, 20, 151, 34)
Quad(151, 20, 165, 34)
Quad(165, 20, 179, 34)
Node B(238, 0, 458, 39)
Quad(299, 4, 313, 18)
Quad(313, 4, 327, 18)
Quad(327, 4, 341, 18)
Quad(341, 4, 355, 18)
Quad(355, 4, 369, 18)
Quad(369, 4, 383, 18)
Quad(383, 4, 397, 18)
Quad(250, 20, 264, 34)
Quad(264, 20, 278, 34)
Quad(278, 20, 292, 34)
Quad(292, 20, 306, 34)
Quad(306, 20, 320, 34)
Quad(320, 20, 334, 34)
Quad(334, 20, 348, 34)
Quad(348, 20, 362, 34)
Quad(362, 20, 376, 34)
Quad(376, 20, 390, 34)
Quad(390, 20, 404, 34)
Quad(404, 20, 418, 34)
Quad(418, 20, 432, 34)
Quad(432, 20, 446, 34)
Node C(46, 114, 219, 169)
Quad(76, 118, 90, 132)
Quad(90, 118, 104, 132)
Quad(104, 118, 118, 132)
Quad(118, 118, 132, 132)
Quad(132, 118, 146, 132)
Quad(146, 118, 160, 132)
Quad(160, 118, 174, 132)
Quad(174, 118, 188, 132)
Quad(83, 134, 97, 148)
Quad(97, 134, 111, 148)
Quad(111, 134, 125, 148)
Quad(125, 134, 139, 148)
Quad(139, 134, 153, 148)
Quad(153, 134, 167, 148)
Quad(167, 134, 181, 148)
Quad(55, 150, 69, 164)
Quad(69, 150, 83, 164)
Quad(83, 150, 97, 164)
Quad(97, 150, 111, 164)
Quad(111, 150, 125, 164)
Quad(125, 150, 139, 164)
Quad(139, 150, 153, 164)
Quad(153, 150, 167, 164)
Quad(167, 150, 181, 164)
Quad(181, 150, 195, 164)
Quad(195, 150, 209, 164)
Node D(269, 122, 442, 161)
Quad(320, 126, 334, 140)
Quad(334, 126, 348, 140)
Quad(348, 126, 362, 140)
Quad(362, 126, 376, 140)
Quad(376, 126, 390, 140)
Quad(278, 142, 292, 156)
Quad(292, 142, 306, 156)
Quad(306, 142, 320, 156)
Quad(320, 142, 334, 156)
Quad(334, 142, 348, 156)
Quad(348, 142, 362, 156)
Quad(362, 142, 376, 156)
Quad(376, 142, 390, 156)
Quad(390, 142, 404, 156)
Quad(404, 142, 418, 156)
Quad(418, 142, 432, 156)
Node E(0, 252, 235, 274)
Quad(12, 256, 26, 270)
Quad(26, 256, 40, 270)
Quad(40, 256, 54, 270)
Quad(54, 256, 68, 270)
Quad(68, 256, 82, 270)
Quad(82, 256, 96, 270)
Quad(96, 256, 110, 270)
Quad(110, 256, 124, 270)
Quad(124, 256, 138, 270)
Quad(138, 256, 152, 270)
Quad(152, 256, 166, 270)
Quad(166, 256, 180, 270)
Quad(180, 256, 194, 270)
Quad(194, 256, 208, 270)
Quad(208, 256, 222, 270)
Node F(285, 244, 489, 283)
Quad(338, 248, 352, 262)
Quad(352, 248, 366, 262)
Quad(366, 248, 380, 262)
Quad(380, 248, 394, 262)
Quad(394, 248, 408, 262)
Quad(408, 248, 422, 262)
Quad(422, 248, 436, 262)
Quad(296, 264, 310, 278)
Quad(310, 264, 324, 278)
Quad(324, 264, 338, 278)
Quad(338, 264, 352, 278)
Quad(352, 264, 366, 278)
Quad(366, 264, 380, 278)
Quad(380, 264, 394, 278)
Quad(394, 264, 408, 278)
Quad(408, 264, 422, 278)
Quad(422, 264, 436, 278)
Quad(436, 264, 450, 278)
Quad(450, 264, 464, 278)
Quad(464, 264, 478, 278)
Node G(158, 358, 331, 380)
Quad(167, 362, 181, 376)
Quad(181, 362, 195, 376)
Quad(195, 362, 209, 376)
Quad(209, 362, 223, 376)
Quad(223, 362, 237, 376)
Quad(237, 362, 251, 376)
Quad(251, 362, 265, 376)
Quad(265, 362, 279, 376)
Quad(279, 362, 293, 376)
Quad(293, 362, 307, 376)
Quad(307, 362, 321, 376)
Edge((83, 39), (83, 39), (103, 114), (103, 114))
Edge((135, 39), (135, 39), (326, 114), (326, 122))
Edge((311, 39), (311, 39), (161, 114), (161, 114))
Edge((384, 39), (384, 39), (384, 114), (384, 122))
Edge((132, 169), (132, 169), (117, 244), (117, 252))
Edge((355, 161), (355, 169), (387, 244), (387, 244))
Edge((117, 274), (117, 283), (215, 358), (215, 358))
Edge((387, 283), (387, 283), (273, 358), (273, 358))
)";
    ASSERT_EQ(expected, s);
}
