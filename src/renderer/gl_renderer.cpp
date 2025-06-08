#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"
#include "punkt/gl_error.hpp"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cassert>
#include <ranges>

using namespace punkt::render;

void GLRenderer::updateZoom(const double factor, double cursor_x, double cursor_y, const double window_width,
                            const double window_height) {
    assert(factor > 0.0f);
    cursor_x -= window_width / 2.0;
    cursor_y -= window_height / 2.0;
    m_camera_x += cursor_x / m_zoom;
    m_camera_y += cursor_y / m_zoom;
    m_zoom *= factor;
    m_camera_x -= cursor_x / m_zoom;
    m_camera_y -= cursor_y / m_zoom;
}

void GLRenderer::resetZoom() {
    m_zoom = 1.0;
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

glyph::GlyphCharInfo transformGciForZoom(const glyph::GlyphCharInfo &gci, double zoom) {
    static GLint value = 0;
    if (!value) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    }
    assert(value > 0);
    zoom = std::min(zoom, 1024.0);
    // limit texture size to either max supported texture size or 1024
    const size_t tex_size = std::min(
        std::min(static_cast<GLint>(static_cast<double>(gci.font_size) * zoom), value - 1),
        1024
    );
    return glyph::GlyphCharInfo(gci.c, tex_size);
}

static void resetGLState() {
    GL_CHECK(glBindVertexArray(0));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glUseProgram(0));
}

void GLRenderer::renderFrame() {
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    double time = glfwGetTime();

    // draw graph/cluster/node shapes (in that order)
    GL_CHECK(glUseProgram(m_nodes_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_nodes_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_nodes_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_nodes_shader, "zoom"), m_zoom));
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_nodes_shader, "is_reversed"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_reversed)));
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_nodes_shader, "is_sideways"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_sideways)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_nodes_shader, "time"), static_cast<GLfloat>(time)));

    // draw graph
    GL_CHECK(glBindVertexArray(m_digraph_quad_buffer));
    GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, 1, 1));

    // draw clusters
    GL_CHECK(glBindVertexArray(m_cluster_quads_buffer));
    GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, 1, static_cast<GLsizei>(m_cluster_quads.size())));

    // draw nodes
    GL_CHECK(glBindVertexArray(m_node_quad_buffer));
    GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, 1, static_cast<GLsizei>(m_node_quads.size())));

    resetGLState();

    // draw edges
    for (const bool is_splines_pass: {false, true}) {
        if (is_splines_pass) {
            GL_CHECK(glUseProgram(m_edge_splines_shader));
        } else {
            GL_CHECK(glUseProgram(m_edges_shader));
        }
        GL_CHECK(glUniform2f(glGetUniformLocation(m_edges_shader, "viewport_size"),
            static_cast<GLfloat>(m_viewport_w), static_cast<GLfloat>(m_viewport_h)));
        GL_CHECK(glUniform2f(glGetUniformLocation(m_edges_shader, "camera_pos"),
            static_cast<GLfloat>(m_camera_x), static_cast<GLfloat>(m_camera_y)));
        GL_CHECK(glUniform1f(glGetUniformLocation(m_edges_shader, "zoom"), m_zoom));
        GL_CHECK(glUniform1i(glGetUniformLocation(m_edges_shader, "is_reversed"),
            static_cast<GLint>(m_dg.m_render_attrs.m_rank_dir.m_is_reversed)));
        GL_CHECK(glUniform1i(glGetUniformLocation(m_edges_shader, "is_sideways"),
            static_cast<GLint>(m_dg.m_render_attrs.m_rank_dir.m_is_sideways)));
        GL_CHECK(glUniform1f(glGetUniformLocation(m_edges_shader, "time"), static_cast<GLfloat>(time)));

        if (is_splines_pass) {
            GL_CHECK(glBindVertexArray(m_edge_splines_buffer));
            GL_CHECK(glDrawArraysInstanced(GL_POINTS, 0, static_cast<GLsizei>(m_edge_spline_points.size()),
                n_spline_divisions));
        } else {
            GL_CHECK(glBindVertexArray(m_edge_lines_buffer));
            GL_CHECK(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_edge_line_points.size())));
        }

        resetGLState();
    }

    // draw arrows
    GL_CHECK(glUseProgram(m_arrows_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_arrows_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_arrows_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_arrows_shader, "zoom"), m_zoom));
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_arrows_shader, "is_reversed"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_reversed)));
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_arrows_shader, "is_sideways"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_sideways)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_arrows_shader, "time"), static_cast<GLfloat>(time)));

    GL_CHECK(glBindVertexArray(m_arrow_triangles_buffer));
    GL_CHECK(glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_edge_arrow_triangles.size())));

    resetGLState();

    // Preload all glyphs (glyph loader, for efficiency, requires us to put it into loading mode, where it messes with
    // our opengl context, and thus we have to separate this out into a preloading stage). This may look weird because
    // it doesn't seem as if we are loading anything, but m_glyph_loader.getGlyph(...) actually populates the glyph
    // loaders internal glyph cache, meaning we won't do any rendering next time we call it (i.e. in the text rendering
    // loop, the next step after this)
    m_glyph_loader.startLoading();
    for (const auto &gci: std::views::keys(m_char_quads)) {
        const auto [c, font_size_zoomed] = transformGciForZoom(gci, m_zoom);
        if (font_size_zoomed == 0) {
            continue;
        }
        m_glyph_loader.getGlyph(c, font_size_zoomed);
    }
    m_glyph_loader.endLoading();

    // draw text
    GL_CHECK(glUseProgram(m_chars_shader));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_chars_shader, "viewport_size"), static_cast<GLfloat>(m_viewport_w),
        static_cast<GLfloat>(m_viewport_h)));
    GL_CHECK(glUniform2f(glGetUniformLocation(m_chars_shader, "camera_pos"), static_cast<GLfloat>(m_camera_x),
        static_cast<GLfloat>(m_camera_y)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_chars_shader, "zoom"), m_zoom));
    GL_CHECK(glUniform1i(glGetUniformLocation(m_chars_shader, "font_texture"), 0)); // Texture unit 0
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_chars_shader, "is_reversed"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_reversed)));
    GL_CHECK(
        glUniform1i(glGetUniformLocation(m_chars_shader, "is_sideways"), static_cast<GLint>(m_dg.m_render_attrs.
            m_rank_dir.m_is_sideways)));
    GL_CHECK(glUniform1f(glGetUniformLocation(m_chars_shader, "time"), static_cast<GLfloat>(time)));

    for (const auto &gci: std::views::keys(m_char_quads)) {
        const auto [c, font_size_zoomed] = transformGciForZoom(gci, m_zoom);
        if (font_size_zoomed == 0) {
            continue;
        }
        // glyph texture is already in glyph cache because of previous load loop
        const GLuint glyph_texture = m_glyph_loader.getGlyph(c, font_size_zoomed).m_texture;

        GL_CHECK(glUniform1ui(glGetUniformLocation(m_chars_shader, "font_size"), static_cast<GLuint>(gci.font_size)));
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, glyph_texture));
        GL_CHECK(glBindVertexArray(m_char_buffers.at(gci)));
        GL_CHECK(glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
            static_cast<GLsizei>(m_char_quads.at(gci).m_instances.size())));
        glBindVertexArray(0);
    }

    resetGLState();

    GL_CRITICAL_CHECK_ALL_ERRORS();
}
