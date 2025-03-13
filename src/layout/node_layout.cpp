#include "punkt/dot.hpp"
#include "punkt/glyph_loader/glyph_loader.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"

#include <ranges>
#include <algorithm>
#include <cassert>
#include <cmath>

constexpr size_t default_font_size = 14;
constexpr float extra_padding_factor = 1.2f;
constexpr std::string_view default_shape = "box";
constexpr size_t min_padding = 4; // pixels

using namespace punkt;

enum class TextAlignment {
    left,
    center,
    right,
};

constexpr auto default_label_just = TextAlignment::center;

static TextAlignment textAlignmentFromStr(const std::string_view &s) {
    if (s == "left") {
        return TextAlignment::left;
    } else if (s == "right") {
        return TextAlignment::right;
    } else {
        return TextAlignment::center;
    }
}

GlyphQuad::GlyphQuad(const size_t left, const size_t top, const size_t right, const size_t bottom, const char32_t c)
    : left(left), top(top), right(right), bottom(bottom), c(c) {
}

void Node::populateRenderInfo(render::glyph::GlyphLoader &glyph_loader) {
    const std::string_view text = m_attrs.contains("label") ? m_attrs.at("label") : m_name;
    const size_t font_size = m_attrs.contains("fontsize")
                                 ? stringViewToSizeT(m_attrs.at("fontsize"), "fontsize")
                                 : default_font_size;
    const TextAlignment ta = m_attrs.contains("labeljust")
                                 ? textAlignmentFromStr(m_attrs.at("labeljust"))
                                 : default_label_just;
    const float margin = m_attrs.contains("margin") ? stringViewToFloat(m_attrs.at("margin"), "margin") : 0.11f;
    const std::string_view shape = m_attrs.contains("shape") ? m_attrs.at("shape") : default_shape;
    const float pen_width = m_attrs.contains("penwidth") ? stringViewToFloat(m_attrs.at("penwidth"), "penwidth") : 1.0f;
    constexpr size_t dpi = DEFAULT_DPI; // TODO handle custom DPI settings

    if (const std::string_view type = m_attrs.contains("@type") ? m_attrs.at("@type") : ""; type == "ghost") {
        assert(m_render_attrs.m_is_ghost);
        m_render_attrs.m_height = 1;
        assert(m_outgoing.size() == 1);
        assert(m_ingoing.size() == 1);

        const Edge &edge_out = m_outgoing.at(0);
        const Edge &edge_in = m_ingoing.at(0);
        const float edge_pen_width = edge_out.m_attrs.contains("penwidth")
                                         ? stringViewToFloat(edge_out.m_attrs.at("penwidth"), "penwidth")
                                         : 1.0f;
        const auto edge_thickness = static_cast<size_t>(
            edge_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

        assert(!edge_out.m_attrs.contains("penwidth") && !edge_in.m_attrs.contains("penwidth") ||
            edge_out.m_attrs.at("penwidth") == edge_in.m_attrs.at("penwidth"));

        m_render_attrs.m_width = edge_thickness;
        return;
    }

    // TODO handle unicode
    std::vector<size_t> quad_lines;
    quad_lines.reserve(text.length());
    m_render_attrs.m_quads.reserve(text.length());

    std::vector<size_t> line_widths;
    line_widths.reserve(std::ranges::count(text, '\n') + 1);

    // build quads but (0, 0) is top left character for now (should later be bounding box of quad)
    const auto f_font_size = static_cast<float>(font_size);
    size_t x = 0, y = font_size, line = 0;
    for (size_t i = 0; i < text.length(); i++) {
        size_t x_prev = x;
        const char c = text.at(i);
        const render::glyph::Glyph &glyph = glyph_loader.getGlyph(c, font_size);
        x += glyph.m_width;
        if (glyph.m_width * glyph.m_height > 0) {
            m_render_attrs.m_quads.emplace_back(x_prev, y - font_size, x, y, c);
        }
        quad_lines.emplace_back(line);
        if (c == '\n') {
            line_widths.emplace_back(x);
            x = 0;
            y = static_cast<size_t>(static_cast<float>(y) + extra_padding_factor * f_font_size);
            line += 1;
        }
    }
    line_widths.emplace_back(x);

    const size_t max_line_width = std::ranges::max(line_widths);
    // apply text alignment (left, center or right)
    if (ta != TextAlignment::left) {
        for (size_t i = 0; i < m_render_attrs.m_quads.size(); i++) {
            const size_t quad_line = quad_lines.at(i);
            const size_t line_width = line_widths.at(quad_line);
            size_t adjustment = max_line_width - line_width;
            if (ta == TextAlignment::center) {
                adjustment /= 2;
            }

            // shift horizontally
            GlyphQuad &gq = m_render_attrs.m_quads.at(i);
            gq.left += adjustment;
            gq.right += adjustment;
        }
    }

    // TODO handle padding for non-rectangular node shapes (ellipse, oval)
    assert(shape == "box" || shape == "rect" || shape == "none");

    auto border_thickness = static_cast<size_t>(pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));
    if (shape == "none") {
        border_thickness = 0;
    }
    // how much to adjust the width and height by based on border thickness.
    // TODO adjust for other shapes
    const auto border_thickness_border_adjustment = 2 * border_thickness;

    // add padding around the text
    m_render_attrs.m_width = static_cast<size_t>(std::round(static_cast<float>(max_line_width) * (1.0f + margin))) +
                             border_thickness_border_adjustment;
    m_render_attrs.m_height = static_cast<size_t>(std::round(static_cast<float>(y) * (1.0f + margin))) +
                              border_thickness_border_adjustment;
    m_render_attrs.m_border_thickness = border_thickness;

    size_t offset_x = (m_render_attrs.m_width - max_line_width) / 2;
    size_t offset_y = (m_render_attrs.m_height - y) / 2;

    if (shape == "box" || shape == "rect") {
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
        gq.left += offset_x;
        gq.top += offset_y;
        gq.right += offset_x;
        gq.bottom += offset_y;
    }
}

void Digraph::computeNodeLayouts(render::glyph::GlyphLoader &glyph_loader) {
    for (Node &node: std::views::values(m_nodes)) {
        node.populateRenderInfo(glyph_loader);
    }
}
