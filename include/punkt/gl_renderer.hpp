#pragma once

#include "dot_constants.hpp"
#include "punkt/dot.hpp"

#include <glad/glad.h>

#include <unordered_map>
#include <span>

namespace punkt::render {
struct CharQuadPerInstanceData {
    GLuint m_x{}, m_y{}, m_color{};

    CharQuadPerInstanceData(GLuint x, GLuint y, GLuint color);
};

// A quad laid out as a triangle strip internally
struct GLQuad {
    GLuint m_points[4 * 2]{};

    GLQuad(size_t x, size_t y, size_t width, size_t height);
};

struct CharQuadInstanceTracker {
    GLQuad m_quad;
    std::vector<CharQuadPerInstanceData> m_instances;

    CharQuadInstanceTracker(size_t width, size_t height);
};

struct NodeQuadInfo {
    GLuint m_top_left_x;
    GLuint m_top_left_y;
    GLuint m_width;
    GLuint m_height;
    GLuint m_fill_color;
    GLuint m_border_color;
    GLuint m_shape_id;
    GLuint m_border_thickness;

    NodeQuadInfo(GLuint top_left_x, GLuint top_left_y, GLuint fill_color, GLuint border_color,
                 GLuint shape_id, GLuint border_thickness, GLuint width, GLuint height);
};

struct EdgeLinePoints {
    Vector2<GLuint> m_points[expected_edge_line_length];
    GLuint m_line_thickness;
    GLuint m_packed_color;

    explicit EdgeLinePoints(std::span<const Vector2<size_t>> points, GLuint packed_color, GLuint line_thickness);
};

struct EdgeArrowTriangle {
    Vector2<GLfloat> m_points[3];
    GLuint m_shape_id;
    GLuint m_packed_color;

    explicit EdgeArrowTriangle(std::span<const Vector2<double>> points, GLuint packed_color, GLuint shape_id);
};

using VAO = GLuint;

struct GLRenderer {
    const Digraph &m_dg;
    glyph::GlyphLoader &m_glyph_loader;
    float m_zoom{};
    size_t m_viewport_w{}, m_viewport_h{};
    double m_camera_x{}, m_camera_y{};
    std::unordered_map<glyph::GlyphCharInfo, CharQuadInstanceTracker, glyph::GlyphCharInfoHasher> m_char_quads;
    std::vector<NodeQuadInfo> m_node_quads;
    std::vector<EdgeLinePoints> m_edge_line_points;
    std::vector<EdgeArrowTriangle> m_edge_arrow_triangles;
    // opengl buffers
    VAO m_node_quad_buffer{}, m_edge_lines_buffer{}, m_arrow_triangles_buffer{};
    std::unordered_map<glyph::GlyphCharInfo, VAO, glyph::GlyphCharInfoHasher> m_char_buffers;
    GLuint m_nodes_shader{}, m_chars_shader{}, m_edges_shader{}, m_arrows_shader{};

    explicit GLRenderer(const Digraph &dg, glyph::GlyphLoader &glyph_loader);

    void notifyFramebufferSize(int width, int height);

    void renderFrame();

    void updateZoom(float factor);

    void resetZoom();

    void notifyCursorMovement(double dx, double dy);

private:
    void buildArrows(const Edge &edge, GLuint edge_color);
};
}
