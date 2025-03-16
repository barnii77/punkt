#include "punkt/glyph_loader/glyph_loader.hpp"

using namespace punkt::render::glyph;

bool GlyphCharInfo::operator==(const GlyphCharInfo &) const = default;

size_t GlyphCharInfoHasher::operator()(const GlyphCharInfo &glyph) const noexcept {
    const size_t h1 = std::hash<char32_t>{}(glyph.c);
    const size_t h2 = std::hash<size_t>{}(glyph.font_size);
    return h1 ^ ((h2 << 13) | (h2 >> (32 - 13)));
}

const Glyph &GlyphLoader::getGlyph(const char32_t c, const size_t font_size) {
    const GlyphCharInfo k(c, font_size);
    if (!m_loaded_glyphs.contains(k)) {
        if (m_is_real_loader) {
            loadGlyph(c, font_size);
        } else {
            // all black and non-transparent (for testing)
            Glyph g(std::vector<uint8_t>(font_size * font_size), font_size, font_size);
            m_loaded_glyphs.insert_or_assign(k, std::move(g));
        }
    }
    return m_loaded_glyphs.at(k);
}

void GlyphLoader::loadGlyph(const char32_t c, const size_t font_size) {
    // TODO
}
