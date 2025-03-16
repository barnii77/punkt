#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"
#include "punkt/gl_error.hpp"
#include "punkt/int_types.hpp"

#include <glad/glad.h>

#include <cassert>
#include <ranges>

using namespace punkt::render;

void GLRenderer::updateZoom(const float factor) {
    assert(factor > 0.0f);
    m_zoom *= factor;
    // const float adjust_factor = (1.0f - 1.0f / factor) / (2.0f * m_zoom);
    // m_camera_x += static_cast<ssize_t>(adjust_factor * static_cast<float>(m_viewport_w));
    // m_camera_y += static_cast<ssize_t>(adjust_factor * static_cast<float>(m_viewport_h));
}

void GLRenderer::resetZoom() {
    m_zoom = 1.0f;
}

void GLRenderer::notifyFramebufferSize(const int width, const int height) {
    m_viewport_w = width;
    m_viewport_h = height;
    GL_CHECK(glViewport(0, 0, width, height));
}

void GLRenderer::notifyCursorMovement(const double dx, const double dy) {
    const double inv_zoom = 1.0 / m_zoom;
    m_camera_x -= inv_zoom * dx;
    m_camera_y -= inv_zoom * dy;
}

glyph::GlyphCharInfo transformGciForZoom(const glyph::GlyphCharInfo &gci, const float zoom) {
    static GLint value = 0;
    if (!value) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    }
    // limit texture size to either max supported texture size or 1024
    const size_t tex_size = std::min(std::min(static_cast<GLint>(static_cast<float>(gci.font_size) * zoom), value - 1),
                                     1024);
    return glyph::GlyphCharInfo(gci.c, tex_size);
}

// TODO spline edges
void GLRenderer::renderFrame() {
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    // draw nodes
    GL_CHECK(glUseProgram(m_nodes_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_nodes_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_nodes_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_nodes_shader, "zoom"), m_zoom));

    GL_CHECK(glBindVertexArray(m_node_quad_buffer));
    GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, 1, static_cast<GLsizei>(m_node_quads.size())));

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // draw edges
    GL_CHECK(glUseProgram(m_edges_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_edges_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_edges_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_edges_shader, "zoom"), m_zoom));

    GL_CHECK(glBindVertexArray(m_edge_lines_buffer));
    GL_CHECK(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_edge_line_points.size())));

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // draw chars
    GL_CHECK(glUseProgram(m_chars_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_chars_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_chars_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_chars_shader, "zoom"), m_zoom));
    GL_CHECK(glUniform1i(glGetUniformLocation(m_chars_shader, "font_texture"), 0)); // Texture unit 0

    for (auto &gci: std::views::keys(m_char_quads)) {
        GL_CHECK(glUniform1ui(glGetUniformLocation(m_chars_shader, "font_size"), static_cast<GLuint>(gci.font_size)));
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, getGlyphTexture(transformGciForZoom(gci, m_zoom))));
        GL_CHECK(glBindVertexArray(m_char_buffers.at(gci)));
        GL_CHECK(glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
            static_cast<GLsizei>(m_char_quads.at(gci).m_instances.size())));
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    GL_CRITICAL_CHECK_ALL_ERRORS();
}
