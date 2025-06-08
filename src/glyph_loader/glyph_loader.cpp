#include "punkt/glyph_loader/glyph_loader.hpp"
#include "punkt/gl_error.hpp"
#include "punkt/dot_constants.hpp"
#include "generated/punkt/shaders/shaders.hpp"

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

void GlyphLoader::loadAndCompileShaders() {
    if (m_font_type == FontType::ttf) {
        m_ttf_stencil_shader = createShaderProgram(glyph_loader_ttf_stencil_shader_vertex_shader_code,
                                                   glyph_loader_ttf_stencil_shader_geometry_shader_code,
                                                   glyph_loader_ttf_stencil_shader_fragment_shader_code);
        m_ttf_cover_shader = createShaderProgram(glyph_loader_ttf_cover_shader_vertex_shader_code,
                                                 glyph_loader_ttf_cover_shader_geometry_shader_code,
                                                 glyph_loader_ttf_cover_shader_fragment_shader_code);
    }
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

    if (m_font_type == FontType::ttf) {
        // enable stencil testing
        GL_CHECK(glEnable(GL_STENCIL_TEST));
        GL_CHECK(glFrontFace(GL_CCW));
    }
}

void GlyphLoader::endLoading() {
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_pre_load_mode_enter_framebuffer));
    GL_CHECK(glDeleteFramebuffers(1, &m_render_framebuffer));
    m_in_load_mode = false;

    if (m_font_type == FontType::ttf) {
        // disable stencil testing
        GL_CHECK(glDisable(GL_STENCIL_TEST));

        // restore default write masks
        GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        GL_CHECK(glDepthMask(GL_TRUE));

        // restore default stencil state (optional, but good hygiene)
        GL_CHECK(glStencilMask(0xFF));
        GL_CHECK(glStencilFunc(GL_ALWAYS, 0, 0xFF));
        GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    }
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

static GLuint newSingleChannelTexture(const size_t width, const size_t height, const std::span<const uint8_t> data) {
    assert(data.empty() || width * height == data.size());
    GLuint texture;
    GL_CHECK(glGenTextures(1, &texture));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, static_cast<GLsizei>(width),
        static_cast<GLsizei>(height), 0, GL_RED, GL_UNSIGNED_BYTE, data.data()));
    if (!data.empty()) {
        GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D)); // for zooming out
    }
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    return texture;
}

static void newFrameBuffer(const size_t width, const size_t height, GLuint &out_texture, GLuint &out_rbo,
                           GLuint &out_fbo) {
    out_texture = newSingleChannelTexture(width, height, std::span<const uint8_t>{});

    // framebuffer
    GL_CHECK(glGenFramebuffers(1, &out_fbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, out_fbo));
    GL_CHECK(glFramebufferTexture2D(
        GL_FRAMEBUFFER, // target
        GL_COLOR_ATTACHMENT0, // attachment point
        GL_TEXTURE_2D, // texture type
        out_texture, // texture ID
        0 // mip level
    ));
    // depth/stencil renderbuffer
    GL_CHECK(glGenRenderbuffers(1, &out_rbo));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, out_rbo));
    GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(width),
        static_cast<GLsizei>(height)));
    GL_CHECK(glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        out_rbo
    ));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        abort();
    }

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, out_fbo));
    GL_CHECK(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));
}

static void deleteFrameBuffer(const GLuint texture, const GLuint rbo, const GLuint fbo) {
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CHECK(glDeleteFramebuffers(1, &fbo));
    GL_CHECK(glDeleteRenderbuffers(1, &rbo));
    GL_CHECK(glDeleteTextures(1, &texture));
}

void GlyphLoader::loadRasterGlyph(const char32_t c, const size_t font_size, const std::span<const uint8_t> glyph_data,
                                  const size_t glyph_width, const size_t glyph_height) {
    assert(m_in_load_mode);
    assert(glyph_width * glyph_height == glyph_data.size());
    GLuint texture = newSingleChannelTexture(glyph_width, glyph_height, glyph_data);
    const GlyphCharInfo gci(c, font_size);
    m_loaded_glyphs.put(gci, std::make_unique<Glyph>(texture, glyph_width, glyph_height));
}

// Renders the glyph splines by emitting a back-facing quad to the left and a front-facing quad to the right of the
// point on the spline.
static void renderGlyphIntoStencil(const GLuint ttf_stencil_shader) {
    // disable color and depth writes
    GL_CHECK(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
    GL_CHECK(glDepthMask(GL_FALSE));

    // make sure all bits in stencil are writable
    GL_CHECK(glStencilMask(0xFF));

    // always pass stencil test, reference=0
    GL_CHECK(glStencilFunc(GL_ALWAYS, 0, 0xFF));

    // front faces increment, back faces decrement (wrap on overflow)
    GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));
    GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));

    // enable proper shader
    GL_CHECK(glUseProgram(ttf_stencil_shader));

    // render
    GL_CHECK(glBindVertexArray(/*TODO*/0));
    GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, static_cast<GLsizei>(/*TODO*/0), punkt::n_spline_divisions));
}

static void renderCoverPassWithStencil(const GLuint quad_vao, const GLuint ttf_cover_shader) {
    // enable proper shader
    GL_CHECK(glUseProgram(ttf_cover_shader));

    // re‑enable color writes, leave depth off
    GL_CHECK(glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE));
    // depth still off
    // glDepthMask(GL_FALSE);

    // only pass where stencil == 0 (inside winding)
    GL_CHECK(glStencilFunc(GL_EQUAL, 0, 0xFF));
    GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
    GL_CHECK(glStencilMask(0x00));

    // render
    GL_CHECK(glBindVertexArray(quad_vao));
    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 6));
}

void GlyphLoader::loadTTFGlyph(const char32_t c, const size_t font_size) {
    if (!m_quad_vao) {
        // TODO move this to constructor or something (where all my opengl stuff is initialized)?
        // initialize quad vao, which contains a single full-view quad
        GLuint buffer;
        GL_CHECK(glGenBuffers(1, &buffer));
        GL_CHECK(glGenVertexArrays(1, &m_quad_vao));
        GL_CHECK(glBindVertexArray(m_quad_vao));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, buffer));
        constexpr GLfloat quad[]{
            /* triangle 1 */ -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
            /* triangle 2 */ -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f
        };
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(sizeof(quad)), quad, GL_STATIC_DRAW));
        GL_CHECK(glEnableVertexAttribArray(0));
        GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(2 * sizeof(GLfloat)), static_cast<void *>(nullptr)));
        GL_CHECK(glBindVertexArray(0));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    // TODO compute glyph_width and glyph_height
    size_t glyph_width{}, glyph_height{};

    GLuint texture, rbo, fbo;
    newFrameBuffer(glyph_width, glyph_height, texture, rbo, fbo);
    renderGlyphIntoStencil(m_ttf_stencil_shader);
    renderCoverPassWithStencil(m_quad_vao, m_ttf_cover_shader);
    deleteFrameBuffer(texture, rbo, fbo);

    // TODO
    throw std::runtime_error("not implemented");
}
