#include "punkt/dot.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <iostream>
#include <ranges>

using namespace punkt;

static void emitRect(std::stringstream &ss, const GlyphQuad &gq) {
    ss << "(" << gq.left << ", " << gq.top << ", " << gq.right << ", " << gq.bottom << ")\n";
}

TEST(preprocessing, NodeLayout) {
    const std::string dot_source = R"(
        digraph IntraNodeLayoutTest {
            A [label="Hello\nWorld ABC\nHi\nAnd more words for you"];
            B [label="Deeply philosophical test text", margin=0, fontsize=7];
            A -> B;
        }
    )";

    // Parse the graph. Let preprocess() compute node ranks and also perform ghost insertion.
    Digraph dg{dot_source};
    render::glyph::GlyphLoader glyph_loader;
    dg.preprocess(glyph_loader);

    std::stringstream ss;
    for (const Node &node: std::views::values(dg.m_nodes)) {
        ss << "Node " << node.m_name;
        emitRect(ss, GlyphQuad(0, 0, node.m_render_attrs.m_width, node.m_render_attrs.m_height, 0));
        for (const GlyphQuad &gq: node.m_render_attrs.m_quads) {
            ss << "Quad";
            emitRect(ss, gq);
        }
    }
    std::string s = ss.str();

    constexpr std::string_view expected = R"(Node A(0, 0, 344, 71)
Quad(130, 4, 144, 18)
Quad(144, 4, 158, 18)
Quad(158, 4, 172, 18)
Quad(172, 4, 186, 18)
Quad(186, 4, 200, 18)
Quad(200, 4, 214, 18)
Quad(102, 20, 116, 34)
Quad(116, 20, 130, 34)
Quad(130, 20, 144, 34)
Quad(144, 20, 158, 34)
Quad(158, 20, 172, 34)
Quad(172, 20, 186, 34)
Quad(186, 20, 200, 34)
Quad(200, 20, 214, 34)
Quad(214, 20, 228, 34)
Quad(228, 20, 242, 34)
Quad(151, 36, 165, 50)
Quad(165, 36, 179, 50)
Quad(179, 36, 193, 50)
Quad(18, 52, 32, 66)
Quad(32, 52, 46, 66)
Quad(46, 52, 60, 66)
Quad(60, 52, 74, 66)
Quad(74, 52, 88, 66)
Quad(88, 52, 102, 66)
Quad(102, 52, 116, 66)
Quad(116, 52, 130, 66)
Quad(130, 52, 144, 66)
Quad(144, 52, 158, 66)
Quad(158, 52, 172, 66)
Quad(172, 52, 186, 66)
Quad(186, 52, 200, 66)
Quad(200, 52, 214, 66)
Quad(214, 52, 228, 66)
Quad(228, 52, 242, 66)
Quad(242, 52, 256, 66)
Quad(256, 52, 270, 66)
Quad(270, 52, 284, 66)
Quad(284, 52, 298, 66)
Quad(298, 52, 312, 66)
Quad(312, 52, 326, 66)
Node B(0, 0, 218, 15)
Quad(4, 4, 11, 11)
Quad(11, 4, 18, 11)
Quad(18, 4, 25, 11)
Quad(25, 4, 32, 11)
Quad(32, 4, 39, 11)
Quad(39, 4, 46, 11)
Quad(46, 4, 53, 11)
Quad(53, 4, 60, 11)
Quad(60, 4, 67, 11)
Quad(67, 4, 74, 11)
Quad(74, 4, 81, 11)
Quad(81, 4, 88, 11)
Quad(88, 4, 95, 11)
Quad(95, 4, 102, 11)
Quad(102, 4, 109, 11)
Quad(109, 4, 116, 11)
Quad(116, 4, 123, 11)
Quad(123, 4, 130, 11)
Quad(130, 4, 137, 11)
Quad(137, 4, 144, 11)
Quad(144, 4, 151, 11)
Quad(151, 4, 158, 11)
Quad(158, 4, 165, 11)
Quad(165, 4, 172, 11)
Quad(172, 4, 179, 11)
Quad(179, 4, 186, 11)
Quad(186, 4, 193, 11)
Quad(193, 4, 200, 11)
Quad(200, 4, 207, 11)
Quad(207, 4, 214, 11)
)";

    ASSERT_EQ(s, expected);
}
