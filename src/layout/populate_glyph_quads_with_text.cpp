#include "punkt/dot.hpp"
#include "punkt/populate_glyph_quads_with_text.hpp"

#include <ranges>
#include <algorithm>

using namespace punkt;

constexpr float extra_padding_factor = 1.2f;

TextAlignment punkt::textAlignmentFromStr(const std::string_view &s) {
    if (s == "left") {
        return TextAlignment::left;
    } else if (s == "right") {
        return TextAlignment::right;
    } else {
        return TextAlignment::center;
    }
}

// helper function to append all the glyph quads for the given text at the given font size and the given text alignment
// with (0, 0) being the top left of the text bounding box
void punkt::populateGlyphQuadsWithText(const std::string_view &text, const size_t font_size, const TextAlignment ta,
              const render::glyph::GlyphLoader &glyph_loader, std::vector<GlyphQuad> &out_quads,
              size_t &out_max_line_width, size_t &out_height, const RankDirConfig rank_dir) {
    std::vector<size_t> quad_lines;
    quad_lines.reserve(text.length());
    out_quads.reserve(text.length());

    std::vector<size_t> line_widths;
    line_widths.reserve(std::ranges::count(text, '\n') + 1);

    // build quads but (0, 0) is top left character for now
    const auto f_font_size = static_cast<float>(font_size);
    size_t x = 0, y = font_size, line = 0;
    // TODO handle unicode
    for (size_t i = 0; i < text.length(); i++) {
        size_t x_prev = x;
        const char c = text.at(i);
        const render::glyph::GlyphMeta glyph = glyph_loader.getGlyphMeta(c, font_size);
        x += glyph.m_width;
        if (glyph.m_width * glyph.m_height > 0) {
            if (rank_dir.m_is_sideways) {
                // flip x and y
                out_quads.emplace_back(y - font_size, x_prev, y, x,
                                       render::glyph::GlyphCharInfo(c, font_size));
            } else {
                out_quads.emplace_back(x_prev, y - font_size, x, y,
                                       render::glyph::GlyphCharInfo(c, font_size));
            }
        }
        quad_lines.emplace_back(line);
        if (c == '\n') {
            line_widths.emplace_back(x);
            x = 0;
            y = static_cast<size_t>(static_cast<float>(y) + extra_padding_factor * f_font_size);
            line += 1;
        }
    }
    out_height = y;
    line_widths.emplace_back(x);

    out_max_line_width = std::ranges::max(line_widths);

    // apply text alignment (left, center or right)
    if (ta != TextAlignment::left) {
        for (size_t i = 0; i < out_quads.size(); i++) {
            const size_t quad_line = quad_lines.at(i);
            const size_t line_width = line_widths.at(quad_line);
            size_t adjustment = out_max_line_width - line_width;
            if (ta == TextAlignment::center) {
                adjustment /= 2;
            }

            // shift horizontally
            GlyphQuad &gq = out_quads.at(i);
            if (rank_dir.m_is_sideways) {
                gq.m_top += adjustment;
                gq.m_bottom += adjustment;
            } else {
                gq.m_left += adjustment;
                gq.m_right += adjustment;
            }
            if (rank_dir.m_is_reversed) {
                if (rank_dir.m_is_sideways) {
                    const size_t left = out_max_line_width - gq.m_top, right = out_max_line_width - gq.m_bottom;
                    gq.m_top = right;
                    gq.m_bottom = left;
                } else {
                    const size_t top = out_height - gq.m_left, bottom = out_height - gq.m_right;
                    gq.m_left = bottom;
                    gq.m_right = top;
                }
            }
        }
    }

    if (rank_dir.m_is_sideways) {
        // must swap width and height if we are rendering sideways (i.e. rankdir=LR or rankdir=RL)
        std::swap(out_height, out_max_line_width);
    }
}
