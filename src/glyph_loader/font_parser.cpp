#include "punkt/glyph_loader/glyph_loader.hpp"
#include "punkt/glyph_loader/psf1_loader.hpp"

#ifdef PUNKT_BAKE_FONT_INTO_EXECUTABLE
#include "generated/punkt/glyph_loader/font_resources.hpp"
#else
#include "punkt/glyph_loader/default_font_resources.hpp"
#endif

#include <glad/glad.h>

#include <fstream>
#include <string>
#include <iterator>
#include <stdexcept>
#include <cassert>
#include <ranges>

using namespace punkt::render::glyph;

GlyphLoader::GlyphLoader() = default;

static void setMaxFontSize(size_t &m_max_allowed_font_size) {
    static GLint value = 0;
    if (!value) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    }
    assert(value > 0);
    // limit texture size to either max supported texture size or 1024
    m_max_allowed_font_size = std::min(value, 1024);
}

GlyphLoader::GlyphLoader(std::string font_path)
    : m_is_real_loader(true), m_font_path(std::move(font_path)) {
    setMaxFontSize(m_max_allowed_font_size);
    if (raw_font_data_map.contains(m_font_path)) {
        m_raw_font_data = raw_font_data_map[m_font_path];
    } else {
        // must load from file
        std::ifstream font_file(m_font_path, std::ios::binary);
        if (!font_file) {
            throw FontNotFoundException(m_font_path);
        }

        font_file.seekg(0, std::ios::end);
        const std::streamsize file_size = font_file.tellg();
        font_file.seekg(0, std::ios::beg);

        m_raw_font_data = std::string(file_size, '\0');
        font_file.read(m_raw_font_data.data(), file_size);
    }
    parseFontData();
}

void GlyphLoader::parseFontData() {
    if (m_raw_font_data.starts_with("\x36\x04")) {
        m_font_type = FontType::psf1;
        parsePSF1(m_font_data, m_raw_font_data);
        for (const auto &font_data = std::get<PSF1GlyphsT>(m_font_data);
             const auto c: std::views::keys(font_data.data)) {
            m_glyph_metas.insert_or_assign(GlyphCharInfo(c, 0), GlyphMeta(font_data.base_width, font_data.base_height));
        }
    } else {
        // TODO
        m_font_type = FontType::ttf;
        throw std::runtime_error("not implemented");
    }
}
