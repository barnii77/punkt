#pragma once

namespace punkt {
enum class TextAlignment {
    left,
    center,
    right,
};

constexpr auto default_label_just = TextAlignment::center;

TextAlignment textAlignmentFromStr(const std::string_view &s);

void populateGlyphQuadsWithText(const std::string_view &text, size_t font_size, TextAlignment ta,
                                const render::glyph::GlyphLoader &glyph_loader, std::vector<GlyphQuad> &out_quads,
                                size_t &out_max_line_width, size_t &out_height);
}
