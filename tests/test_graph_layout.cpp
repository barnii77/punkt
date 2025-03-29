#include "punkt/dot.hpp"
#include "punkt/dot_constants.hpp"
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
    const size_t og_barycenter_x_optimization_max_iters = BARYCENTER_X_OPTIMIZATION_MAX_ITERS;
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = 0;
    dg.preprocess(glyph_loader);
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = og_barycenter_x_optimization_max_iters;

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
                     node.m_render_attrs.m_x + gq.m_left,
                     node.m_render_attrs.m_y + gq.m_top,
                     node.m_render_attrs.m_x + gq.m_right,
                     node.m_render_attrs.m_y + gq.m_bottom);
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
            A -> G;
        }
    )";

    // Parse the graph. Let preprocess() compute node ranks and perform ghost insertion.
    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    const size_t og_barycenter_x_optimization_max_iters = BARYCENTER_X_OPTIMIZATION_MAX_ITERS;
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = 0;
    dg.preprocess(glyph_loader);
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = og_barycenter_x_optimization_max_iters;

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
                     node.m_render_attrs.m_x + gq.m_left,
                     node.m_render_attrs.m_y + gq.m_top,
                     node.m_render_attrs.m_x + gq.m_right,
                     node.m_render_attrs.m_y + gq.m_bottom);
        }
    }

    for (const Node &node: std::views::values(dg.m_nodes)) {
        for (const Edge &edge: node.m_outgoing) {
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

    constexpr std::string_view expected = R"(Graph(540, 380)
RankSep=75 NodeSep=50
Rank 0(56, 0, 427, 39)
Rank 1(46, 114, 447, 55)
Rank 2(0, 244, 540, 39)
Rank 3(183, 358, 173, 22)
Node A(56, 0, 213, 39)
Quad(85, 4, 99, 18)
Quad(99, 4, 113, 18)
Quad(113, 4, 127, 18)
Quad(127, 4, 141, 18)
Quad(141, 4, 155, 18)
Quad(155, 4, 169, 18)
Quad(169, 4, 183, 18)
Quad(64, 20, 78, 34)
Quad(78, 20, 92, 34)
Quad(92, 20, 106, 34)
Quad(106, 20, 120, 34)
Quad(120, 20, 134, 34)
Quad(134, 20, 148, 34)
Quad(148, 20, 162, 34)
Quad(162, 20, 176, 34)
Quad(176, 20, 190, 34)
Quad(190, 20, 204, 34)
Node B(263, 0, 483, 39)
Quad(324, 4, 338, 18)
Quad(338, 4, 352, 18)
Quad(352, 4, 366, 18)
Quad(366, 4, 380, 18)
Quad(380, 4, 394, 18)
Quad(394, 4, 408, 18)
Quad(408, 4, 422, 18)
Quad(275, 20, 289, 34)
Quad(289, 20, 303, 34)
Quad(303, 20, 317, 34)
Quad(317, 20, 331, 34)
Quad(331, 20, 345, 34)
Quad(345, 20, 359, 34)
Quad(359, 20, 373, 34)
Quad(373, 20, 387, 34)
Quad(387, 20, 401, 34)
Quad(401, 20, 415, 34)
Quad(415, 20, 429, 34)
Quad(429, 20, 443, 34)
Quad(443, 20, 457, 34)
Quad(457, 20, 471, 34)
Node C(97, 114, 270, 169)
Quad(127, 118, 141, 132)
Quad(141, 118, 155, 132)
Quad(155, 118, 169, 132)
Quad(169, 118, 183, 132)
Quad(183, 118, 197, 132)
Quad(197, 118, 211, 132)
Quad(211, 118, 225, 132)
Quad(225, 118, 239, 132)
Quad(134, 134, 148, 148)
Quad(148, 134, 162, 148)
Quad(162, 134, 176, 148)
Quad(176, 134, 190, 148)
Quad(190, 134, 204, 148)
Quad(204, 134, 218, 148)
Quad(218, 134, 232, 148)
Quad(106, 150, 120, 164)
Quad(120, 150, 134, 164)
Quad(134, 150, 148, 164)
Quad(148, 150, 162, 164)
Quad(162, 150, 176, 164)
Quad(176, 150, 190, 164)
Quad(190, 150, 204, 164)
Quad(204, 150, 218, 164)
Quad(218, 150, 232, 164)
Quad(232, 150, 246, 164)
Quad(246, 150, 260, 164)
Node D(320, 122, 493, 161)
Quad(371, 126, 385, 140)
Quad(385, 126, 399, 140)
Quad(399, 126, 413, 140)
Quad(413, 126, 427, 140)
Quad(427, 126, 441, 140)
Quad(329, 142, 343, 156)
Quad(343, 142, 357, 156)
Quad(357, 142, 371, 156)
Quad(371, 142, 385, 156)
Quad(385, 142, 399, 156)
Quad(399, 142, 413, 156)
Quad(413, 142, 427, 156)
Quad(427, 142, 441, 156)
Quad(441, 142, 455, 156)
Quad(455, 142, 469, 156)
Quad(469, 142, 483, 156)
Node E(51, 252, 286, 274)
Quad(63, 256, 77, 270)
Quad(77, 256, 91, 270)
Quad(91, 256, 105, 270)
Quad(105, 256, 119, 270)
Quad(119, 256, 133, 270)
Quad(133, 256, 147, 270)
Quad(147, 256, 161, 270)
Quad(161, 256, 175, 270)
Quad(175, 256, 189, 270)
Quad(189, 256, 203, 270)
Quad(203, 256, 217, 270)
Quad(217, 256, 231, 270)
Quad(231, 256, 245, 270)
Quad(245, 256, 259, 270)
Quad(259, 256, 273, 270)
Node F(336, 244, 540, 283)
Quad(389, 248, 403, 262)
Quad(403, 248, 417, 262)
Quad(417, 248, 431, 262)
Quad(431, 248, 445, 262)
Quad(445, 248, 459, 262)
Quad(459, 248, 473, 262)
Quad(473, 248, 487, 262)
Quad(347, 264, 361, 278)
Quad(361, 264, 375, 278)
Quad(375, 264, 389, 278)
Quad(389, 264, 403, 278)
Quad(403, 264, 417, 278)
Quad(417, 264, 431, 278)
Quad(431, 264, 445, 278)
Quad(445, 264, 459, 278)
Quad(459, 264, 473, 278)
Quad(473, 264, 487, 278)
Quad(487, 264, 501, 278)
Quad(501, 264, 515, 278)
Quad(515, 264, 529, 278)
Node G(183, 358, 356, 380)
Quad(192, 362, 206, 376)
Quad(206, 362, 220, 376)
Quad(220, 362, 234, 376)
Quad(234, 362, 248, 376)
Quad(248, 362, 262, 376)
Quad(262, 362, 276, 376)
Quad(276, 362, 290, 376)
Quad(290, 362, 304, 376)
Quad(304, 362, 318, 376)
Quad(318, 362, 332, 376)
Quad(332, 362, 346, 376)
Edge((134, 39), (134, 39), (154, 114), (154, 114))
Edge((173, 39), (173, 39), (377, 114), (377, 122))
Edge()
Edge((95, 39), (95, 39), (46, 114), (46, 141))
Edge((46, 142), (46, 169), (0, 244), (0, 263))
Edge((336, 39), (336, 39), (212, 114), (212, 114))
Edge((409, 39), (409, 39), (435, 114), (435, 122))
Edge((183, 169), (183, 169), (168, 244), (168, 252))
Edge((406, 161), (406, 169), (438, 244), (438, 244))
Edge((168, 274), (168, 283), (269, 358), (269, 358))
Edge((438, 283), (438, 283), (312, 358), (312, 358))
Edge((0, 264), (0, 283), (226, 358), (226, 358))
)";
    ASSERT_EQ(expected, s);
}

TEST(preprocessing, GraphLayoutWithEdgeLabels) {
    const std::string dot_source = R"(
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

    // Parse the graph. Let preprocess() compute node ranks and perform ghost insertion.
    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    const size_t og_barycenter_x_optimization_max_iters = BARYCENTER_X_OPTIMIZATION_MAX_ITERS;
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = 0;
    dg.preprocess(glyph_loader);
    BARYCENTER_X_OPTIMIZATION_MAX_ITERS = og_barycenter_x_optimization_max_iters;

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
                     node.m_render_attrs.m_x + gq.m_left,
                     node.m_render_attrs.m_y + gq.m_top,
                     node.m_render_attrs.m_x + gq.m_right,
                     node.m_render_attrs.m_y + gq.m_bottom);
        }
    }

    for (const Node &node: std::views::values(dg.m_nodes)) {
        for (const Edge &edge: node.m_outgoing) {
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
            const std::vector<GlyphQuad> *collections[] = {
                &edge.m_render_attrs.m_label_quads, &edge.m_render_attrs.m_head_label_quads,
                &edge.m_render_attrs.m_tail_label_quads
            };
            for (size_t i = 0; i < std::size(collections); i++) {
                const std::string_view qname = i == 0 ? "LabelQuad" : i == 1 ? "HeadLabelQuad" : "TailLabelQuad";
                for (const std::vector<GlyphQuad> &quads = *collections[i]; const auto &gq: quads) {
                    ss << qname;
                    emitRect(ss,
                             node.m_render_attrs.m_x + gq.m_left,
                             node.m_render_attrs.m_y + gq.m_top,
                             node.m_render_attrs.m_x + gq.m_right,
                             node.m_render_attrs.m_y + gq.m_bottom);
                }
            }
        }
    }

    std::string s = ss.str();

    constexpr std::string_view expected = R"(Graph(540, 384)
RankSep=75 NodeSep=50
Rank 0(67, 0, 405, 39)
Rank 1(46, 114, 447, 55)
Rank 2(0, 244, 540, 39)
Rank 3(179, 358, 181, 26)
Node A(67, 2, 202, 37)
Quad(92, 6, 104, 18)
Quad(104, 6, 116, 18)
Quad(116, 6, 128, 18)
Quad(128, 6, 140, 18)
Quad(140, 6, 152, 18)
Quad(152, 6, 164, 18)
Quad(164, 6, 176, 18)
Quad(74, 20, 86, 32)
Quad(86, 20, 98, 32)
Quad(98, 20, 110, 32)
Quad(110, 20, 122, 32)
Quad(122, 20, 134, 32)
Quad(134, 20, 146, 32)
Quad(146, 20, 158, 32)
Quad(158, 20, 170, 32)
Quad(170, 20, 182, 32)
Quad(182, 20, 194, 32)
Node B(252, 0, 472, 39)
Quad(313, 4, 327, 18)
Quad(327, 4, 341, 18)
Quad(341, 4, 355, 18)
Quad(355, 4, 369, 18)
Quad(369, 4, 383, 18)
Quad(383, 4, 397, 18)
Quad(397, 4, 411, 18)
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
Quad(446, 20, 460, 34)
Node C(97, 114, 270, 169)
Quad(127, 118, 141, 132)
Quad(141, 118, 155, 132)
Quad(155, 118, 169, 132)
Quad(169, 118, 183, 132)
Quad(183, 118, 197, 132)
Quad(197, 118, 211, 132)
Quad(211, 118, 225, 132)
Quad(225, 118, 239, 132)
Quad(134, 134, 148, 148)
Quad(148, 134, 162, 148)
Quad(162, 134, 176, 148)
Quad(176, 134, 190, 148)
Quad(190, 134, 204, 148)
Quad(204, 134, 218, 148)
Quad(218, 134, 232, 148)
Quad(106, 150, 120, 164)
Quad(120, 150, 134, 164)
Quad(134, 150, 148, 164)
Quad(148, 150, 162, 164)
Quad(162, 150, 176, 164)
Quad(176, 150, 190, 164)
Quad(190, 150, 204, 164)
Quad(204, 150, 218, 164)
Quad(218, 150, 232, 164)
Quad(232, 150, 246, 164)
Quad(246, 150, 260, 164)
Node D(320, 122, 493, 161)
Quad(371, 126, 385, 140)
Quad(385, 126, 399, 140)
Quad(399, 126, 413, 140)
Quad(413, 126, 427, 140)
Quad(427, 126, 441, 140)
Quad(329, 142, 343, 156)
Quad(343, 142, 357, 156)
Quad(357, 142, 371, 156)
Quad(371, 142, 385, 156)
Quad(385, 142, 399, 156)
Quad(399, 142, 413, 156)
Quad(413, 142, 427, 156)
Quad(427, 142, 441, 156)
Quad(441, 142, 455, 156)
Quad(455, 142, 469, 156)
Quad(469, 142, 483, 156)
Node E(51, 252, 286, 274)
Quad(63, 256, 77, 270)
Quad(77, 256, 91, 270)
Quad(91, 256, 105, 270)
Quad(105, 256, 119, 270)
Quad(119, 256, 133, 270)
Quad(133, 256, 147, 270)
Quad(147, 256, 161, 270)
Quad(161, 256, 175, 270)
Quad(175, 256, 189, 270)
Quad(189, 256, 203, 270)
Quad(203, 256, 217, 270)
Quad(217, 256, 231, 270)
Quad(231, 256, 245, 270)
Quad(245, 256, 259, 270)
Quad(259, 256, 273, 270)
Node F(336, 244, 540, 283)
Quad(389, 248, 403, 262)
Quad(403, 248, 417, 262)
Quad(417, 248, 431, 262)
Quad(431, 248, 445, 262)
Quad(445, 248, 459, 262)
Quad(459, 248, 473, 262)
Quad(473, 248, 487, 262)
Quad(347, 264, 361, 278)
Quad(361, 264, 375, 278)
Quad(375, 264, 389, 278)
Quad(389, 264, 403, 278)
Quad(403, 264, 417, 278)
Quad(417, 264, 431, 278)
Quad(431, 264, 445, 278)
Quad(445, 264, 459, 278)
Quad(459, 264, 473, 278)
Quad(473, 264, 487, 278)
Quad(487, 264, 501, 278)
Quad(501, 264, 515, 278)
Quad(515, 264, 529, 278)
Node G(179, 358, 360, 384)
Quad(192, 364, 206, 378)
Quad(206, 364, 220, 378)
Quad(220, 364, 234, 378)
Quad(234, 364, 248, 378)
Quad(248, 364, 262, 378)
Quad(262, 364, 276, 378)
Quad(276, 364, 290, 378)
Quad(290, 364, 304, 378)
Quad(304, 364, 318, 378)
Quad(318, 364, 332, 378)
Quad(332, 364, 346, 378)
Edge((134, 37), (134, 39), (154, 114), (154, 114))
LabelQuad(225, 71, 239, 85)
LabelQuad(239, 71, 253, 85)
Edge((168, 37), (168, 39), (377, 114), (377, 122))
Edge()
Edge((100, 37), (100, 39), (46, 114), (46, 141))
TailLabelQuad(160, 39, 167, 46)
TailLabelQuad(167, 39, 174, 46)
TailLabelQuad(174, 39, 181, 46)
Edge((46, 142), (46, 169), (0, 244), (0, 263))
LabelQuad(62, 344, 69, 351)
LabelQuad(69, 344, 76, 351)
LabelQuad(76, 344, 83, 351)
Edge((325, 39), (325, 39), (212, 114), (212, 114))
HeadLabelQuad(450, 100, 464, 114)
HeadLabelQuad(464, 100, 478, 114)
Edge((398, 39), (398, 39), (435, 114), (435, 122))
Edge((183, 169), (183, 169), (168, 244), (168, 252))
LabelQuad(259, 313, 273, 327)
LabelQuad(273, 313, 287, 327)
TailLabelQuad(266, 283, 280, 297)
TailLabelQuad(280, 283, 294, 297)
Edge((406, 161), (406, 169), (438, 244), (438, 244))
Edge((168, 274), (168, 283), (269, 358), (269, 358))
Edge((438, 283), (438, 283), (314, 358), (314, 360))
Edge((0, 264), (0, 283), (224, 358), (224, 360))
HeadLabelQuad(231, 616, 238, 623)
HeadLabelQuad(238, 616, 245, 623)
HeadLabelQuad(245, 616, 252, 623)
)";
    ASSERT_EQ(expected, s);
}
