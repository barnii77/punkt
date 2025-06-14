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

struct ShapeQuadInfo {
    GLuint m_top_left_x;
    GLuint m_top_left_y;
    GLuint m_width;
    GLuint m_height;
    GLuint m_fill_color;
    GLuint m_border_color;
    GLuint m_pulsing_color;
    GLuint m_shape_id;
    GLuint m_border_thickness;
    GLfloat m_rotation_speed;
    GLfloat m_pulsing_speed;
    GLfloat m_pulsing_time_offset;
    GLfloat m_pulsing_time_offset_gradient_x;
    GLfloat m_pulsing_time_offset_gradient_y;

    ShapeQuadInfo(GLuint top_left_x, GLuint top_left_y, GLuint fill_color, GLuint border_color, GLuint m_pulsing_color,
                  GLuint shape_id, GLuint border_thickness, GLuint width, GLuint height, GLfloat rotation_speed,
                  GLfloat pulsing_speed, GLfloat pulsing_time_offset, GLfloat pulsing_time_offset_gradient_x,
                  GLfloat pulsing_time_offset_gradient_y);
};

struct EdgeLinePoints {
    Vector2<GLuint> m_points[expected_edge_line_length];
    GLuint m_line_thickness;
    GLuint m_packed_color;
    GLuint m_edge_style;
    GLfloat m_total_curve_length;

    explicit EdgeLinePoints(std::span<const Vector2<size_t>> points, GLuint packed_color, GLuint line_thickness,
                            GLuint edge_style);
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
    double m_zoom{};
    size_t m_viewport_w{}, m_viewport_h{};
    double m_camera_x{}, m_camera_y{};
    std::unordered_map<glyph::GlyphCharInfo, CharQuadInstanceTracker, glyph::GlyphCharInfoHasher> m_char_quads;
    ShapeQuadInfo m_digraph_quad;
    std::vector<ShapeQuadInfo> m_cluster_quads;
    std::vector<ShapeQuadInfo> m_node_quads;
    std::vector<EdgeLinePoints> m_edge_line_points;
    std::vector<EdgeLinePoints> m_edge_spline_points;
    std::vector<EdgeArrowTriangle> m_edge_arrow_triangles;
    // opengl buffers
    VAO m_node_quad_buffer{}, m_digraph_quad_buffer{}, m_cluster_quads_buffer{}, m_edge_lines_buffer{},
            m_edge_splines_buffer{}, m_arrow_triangles_buffer{};
    std::unordered_map<glyph::GlyphCharInfo, VAO, glyph::GlyphCharInfoHasher> m_char_buffers;
    GLuint m_nodes_shader{}, m_chars_shader{}, m_edges_shader{}, m_edge_splines_shader{}, m_arrows_shader{};

    explicit GLRenderer(const Digraph &dg, glyph::GlyphLoader &glyph_loader);

    void notifyFramebufferSize(int width, int height);

    void renderFrame();

    void updateZoom(double factor, double cursor_x, double cursor_y, double window_width, double window_height);

    void resetZoom();

    void notifyCursorMovement(double dx, double dy);

private:
    void buildArrows(const Edge &edge, GLuint edge_color);
};
}
