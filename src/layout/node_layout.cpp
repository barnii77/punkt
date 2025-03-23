#include "punkt/dot.hpp"
#include "punkt/glyph_loader/glyph_loader.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/populate_glyph_quads_with_text.hpp"

#include <ranges>
#include <cassert>
#include <cmath>

constexpr size_t min_padding = 4; // pixels

using namespace punkt;

GlyphQuad::GlyphQuad(const size_t left, const size_t top, const size_t right, const size_t bottom,
                     const render::glyph::GlyphCharInfo c)
    : m_left(left), m_top(top), m_right(right), m_bottom(bottom), m_c(c) {
}

void Node::populateRenderInfo(const render::glyph::GlyphLoader &glyph_loader, const RankDirConfig rank_dir) {
    const std::string_view text = getAttrOrDefault(m_attrs, "label", m_name);
    const size_t font_size = getAttrTransformedCheckedOrDefault(m_attrs, "fontsize", default_font_size,
                                                                stringViewToSizeT);
    const TextAlignment ta =
            getAttrTransformedOrDefault(m_attrs, "labeljust", default_label_just, textAlignmentFromStr);
    const float margin = getAttrTransformedCheckedOrDefault(m_attrs, "margin", 0.11f, stringViewToFloat);
    const std::string_view shape = getAttrOrDefault(m_attrs, "shape", default_shape);
    const float pen_width = getAttrTransformedCheckedOrDefault(m_attrs, "penwidth", 1.0f, stringViewToFloat);
    constexpr size_t dpi = DEFAULT_DPI; // TODO handle custom DPI settings

    if (const std::string_view type = getAttrOrDefault(m_attrs, "@type", ""); type == "ghost") {
        assert(m_render_attrs.m_is_ghost);
        m_render_attrs.m_height = 1;
        assert(m_outgoing.size() == 1);
        assert(m_ingoing.size() == 1);

        const Edge &edge_out = m_outgoing.at(0);
        const Edge &edge_in = m_ingoing.at(0);
        const float edge_pen_width = getAttrTransformedCheckedOrDefault(edge_out.m_attrs, "penwidth", 1.0f,
                                                                        stringViewToFloat);
        // TODO handle custom dpi
        const auto edge_thickness = static_cast<size_t>(
            edge_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

        assert(!edge_out.m_attrs.contains("penwidth") && !edge_in.m_attrs.contains("penwidth") ||
            edge_out.m_attrs.at("penwidth") == edge_in.m_attrs.at("penwidth"));

        m_render_attrs.m_width = edge_thickness;
        return;
    }

    size_t max_line_width, y;
    populateGlyphQuadsWithText(text, font_size, ta, glyph_loader, m_render_attrs.m_quads, max_line_width, y, rank_dir);

    // TODO handle padding for non-rectangular node shapes (ellipse, oval)
    assert(shape == "none" || shape == "box" || shape == "rect" || shape == "ellipse" || shape == "circle");

    auto border_thickness = static_cast<size_t>(pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));
    if (shape == "none") {
        border_thickness = 0;
    }
    // how much to adjust the width and height by based on border thickness.
    // TODO adjust for other shapes
    const auto border_thickness_size_adjustment = 2 * border_thickness;

    // add padding around the text
    m_render_attrs.m_width = static_cast<size_t>(std::round(static_cast<float>(max_line_width) * (1.0f + margin))) +
                             border_thickness_size_adjustment;
    m_render_attrs.m_height = static_cast<size_t>(std::round(static_cast<float>(y) * (1.0f + margin))) +
                              border_thickness_size_adjustment;
    m_render_attrs.m_border_thickness = border_thickness;

    if (shape == "circle") {
        m_render_attrs.m_width = std::max(m_render_attrs.m_width, m_render_attrs.m_height);
        m_render_attrs.m_height = m_render_attrs.m_width;
    }

    size_t offset_x = (m_render_attrs.m_width - max_line_width) / 2;
    size_t offset_y = (m_render_attrs.m_height - y) / 2;

    if (shape == "box" || shape == "rect" || shape == "ellipse" || shape == "circle") {
        if (offset_x < min_padding) {
            m_render_attrs.m_width += 2 * (min_padding - offset_x);
            offset_x = min_padding;
        }
        if (offset_y < min_padding) {
            m_render_attrs.m_height += 2 * (min_padding - offset_y);
            offset_y = min_padding;
        }
    }

    for (GlyphQuad &gq: m_render_attrs.m_quads) {
        gq.m_left += offset_x;
        gq.m_top += offset_y;
        gq.m_right += offset_x;
        gq.m_bottom += offset_y;
    }
}

void Digraph::computeNodeLayouts(const render::glyph::GlyphLoader &glyph_loader) {
    for (Node &node: std::views::values(m_nodes)) {
        node.populateRenderInfo(glyph_loader, m_render_attrs.m_rank_dir);
    }
}
