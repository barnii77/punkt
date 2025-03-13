#include "punkt/glyph_loader/glyph_loader.hpp"

#ifdef PUNKT_BAKE_FONT_INTO_EXECUTABLE
#include "generated/punkt/glyph_loader/font_resources.hpp"
#else
#include "punkt/glyph_loader/default_font_resources.hpp"
#endif

#include <fstream>
#include <string>
#include <iterator>

using namespace render::glyph;

GlyphLoader::GlyphLoader() = default;

GlyphLoader::GlyphLoader(std::string font_path)
    : m_is_real_loader(true), m_font_path(std::move(font_path)) {
    if (raw_font_data_map.contains(m_font_path)) {
        m_raw_font_data = raw_font_data_map[m_font_path];
    } else {
        // must load from file
        std::ifstream font_file(font_path, std::ios::binary);
        if (font_file.fail()) {
            throw FontNotFoundException(m_font_path);
        }
        m_raw_font_data = std::string((std::istreambuf_iterator(font_file)), std::istreambuf_iterator<char>());
    }
    parseFontData();
}

void GlyphLoader::parseFontData() {
    // TODO
}
