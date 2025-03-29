#include "punkt/glyph_loader/glyph_loader.hpp"
#include "punkt/gl_error.hpp"

#include <glad/glad.h>

#include <cassert>
#include <memory>
#include <cmath>
#include <span>

using namespace punkt::render::glyph;

bool GlyphCharInfo::operator==(const GlyphCharInfo &) const = default;

size_t GlyphCharInfoHasher::operator()(const GlyphCharInfo &glyph) const noexcept {
    const size_t h1 = std::hash<char32_t>{}(glyph.c);
    const size_t h2 = std::hash<size_t>{}(glyph.font_size);
    return h1 ^ ((h2 << 13) | (h2 >> (32 - 13)));
}

GlyphMeta GlyphLoader::getGlyphMeta(const char32_t c, const size_t font_size) {
    if (font_size == 0 || font_size > m_max_allowed_font_size) {
        throw IllegalFontSizeException(font_size);
    }
    if (!m_is_real_loader) {
        return {font_size, font_size};
    }
    if (m_font_type == FontType::psf1) {
        const auto &psf1_glyphs = std::get<PSF1GlyphsT>(m_font_data);
        const GlyphCharInfo k(c, 0);
        GlyphMeta glyph_meta{0, 0};
        if (!m_glyph_metas.contains(k)) {
            constexpr GlyphCharInfo k_null('\0', 0);
            glyph_meta = m_glyph_metas.at(k_null);
        } else {
            glyph_meta = m_glyph_metas.at(k);
        }
        const float scaling_factor = static_cast<float>(font_size) / static_cast<float>(std::max(
                                         psf1_glyphs.base_width, psf1_glyphs.base_height));
        glyph_meta.m_width = static_cast<size_t>(std::round(static_cast<float>(glyph_meta.m_width) * scaling_factor));
        glyph_meta.m_width = static_cast<size_t>(std::round(static_cast<float>(glyph_meta.m_height) * scaling_factor));
        return glyph_meta;
    } else if (m_font_type == FontType::ttf) {
        // TODO
        throw std::runtime_error("not implemented");
    } else {
        throw FontParsingException("You have to parse the font before requesting data");
    }
}

const Glyph &GlyphLoader::getGlyph(const char32_t c, const size_t font_size) {
    if (font_size == 0 || font_size > m_max_allowed_font_size) {
        throw IllegalFontSizeException(font_size);
    }
    const GlyphCharInfo k(c, font_size);
    if (!m_loaded_glyphs.contains(k)) {
        loadGlyph(c, font_size);
    }
    return m_loaded_glyphs.at(k);
}

void GlyphLoader::startLoading() {
    m_in_load_mode = true;
    GLint fb;
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb));
    m_pre_load_mode_enter_framebuffer = fb;
    GL_CHECK(glGenFramebuffers(1, &m_render_framebuffer));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_render_framebuffer));
}

void GlyphLoader::endLoading() {
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_pre_load_mode_enter_framebuffer));
    GL_CHECK(glDeleteFramebuffers(1, &m_render_framebuffer));
    m_in_load_mode = false;
}

void GlyphLoader::loadGlyph(const char32_t c, const size_t font_size) {
    if (m_font_type == FontType::ttf) {
        loadTTFGlyph(c, font_size);
    } else if (!m_is_real_loader || m_font_type == FontType::psf1) {
        std::span<const uint8_t> buf;
        size_t width, height;
        if (m_is_real_loader && std::get<PSF1GlyphsT>(m_font_data).data.contains(c)) {
            const auto &psf1_glyph = std::get<PSF1GlyphsT>(m_font_data);
            buf = psf1_glyph.data.at(c);
            width = psf1_glyph.base_width;
            height = psf1_glyph.base_height;
        } else {
            const GlyphCharInfo gci(c, 0);
            const GlyphMeta glyph_meta = m_glyph_metas.contains(gci)
                                             ? m_glyph_metas.at(gci)
                                             : GlyphMeta{font_size, font_size};
            m_zeros_buffer.resize(glyph_meta.m_width * glyph_meta.m_height);
            buf = m_zeros_buffer;
            width = glyph_meta.m_width;
            height = glyph_meta.m_height;
        }
        loadRasterGlyph(c, font_size, buf, width, height);
    } else {
        throw FontParsingException("Illegal font type: " + std::to_string(static_cast<uint32_t>(m_font_type)));
    }
}

void GlyphLoader::loadRasterGlyph(const char32_t c, const size_t font_size, const std::span<const uint8_t> glyph_data,
                                  const size_t glyph_width, const size_t glyph_height) {
    assert(m_in_load_mode);
    assert(glyph_width * glyph_height == glyph_data.size());

    GLuint texture;
    GL_CHECK(glGenTextures(1, &texture));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, static_cast<GLsizei>(glyph_width),
        static_cast<GLsizei>(glyph_height), 0, GL_RED, GL_UNSIGNED_BYTE, glyph_data.data()));
    GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D)); // for zooming out
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    const GlyphCharInfo gci(c, font_size);
    m_loaded_glyphs.put(gci, std::make_unique<Glyph>(texture, glyph_width, glyph_height));
}

void GlyphLoader::loadTTFGlyph(char32_t c, size_t font_size) {
    // TODO
    throw std::runtime_error("not implemented");
}
