#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"
#include "punkt/gl_error.hpp"
#include "punkt/utils/utils.hpp"
#include "punkt/utils/int_types.hpp"
#include "punkt/dot_constants.hpp"
#include "generated/punkt/shaders/shaders.hpp"

#include <ranges>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace punkt;
using namespace punkt::render;

static GLuint getPackedColorFromName(const std::string_view &font_color_str) {
    uint8_t r, g, b, a;
    parseColor(font_color_str, r, g, b, a);
    return (static_cast<GLuint>(a) << 0) | (static_cast<GLuint>(r) << 8) | (static_cast<GLuint>(g) << 16) | (
               static_cast<GLuint>(b) << 24);
}

static GLuint getPackedColorFromAttrs(const Attrs &attrs, const std::string_view attr_name,
                                      const std::string_view default_value) {
    const std::string_view &font_color_str = attrs.contains(attr_name) ? attrs.at(attr_name) : default_value;
    return getPackedColorFromName(font_color_str);
}

constexpr GLuint edge_style_solid = 0;
constexpr GLuint edge_style_dotted = 1;
constexpr GLuint edge_style_dashed = 2;
constexpr GLuint edge_style_bullet = 3;

static GLuint getEdgeStyleId(const std::string_view style) {
    if (style == "dotted") {
        return edge_style_dotted;
    } else if (style == "dashed") {
        return edge_style_dashed;
    } else if (style == "bullet") {
        return edge_style_bullet;
    }
    // default to solid if unrecognized
    return edge_style_solid;
}

constexpr GLuint node_shape_none = 0;
constexpr GLuint node_shape_box = 1;
constexpr GLuint node_shape_ellipse = 2;
constexpr GLuint node_shape_circle = node_shape_ellipse; // from shader POV there is no difference
constexpr GLuint node_shape_pill = 3; // this one is punkt-specific

// used in the fragment shader for identifying how to draw the border
static GLuint getNodeShapeId(const Node &node) {
    if (const std::string_view &shape = getAttrOrDefault(node.m_attrs, "shape", default_shape); shape == "none") {
        return node_shape_none;
    } else if (shape == "ellipse") {
        return node_shape_ellipse;
    } else if (shape == "circle") {
        return node_shape_circle;
    } else if (shape == "pill") {
        return node_shape_pill;
    }
    return node_shape_box;
}

CharQuadPerInstanceData::CharQuadPerInstanceData(const GLuint x, const GLuint y, const GLuint color)
    : m_x(x), m_y(y), m_color(color) {
}

GLQuad::GLQuad(const size_t x, const size_t y, const size_t width, const size_t height) {
    // Create Triangle Strip
    // Bottom-left
    m_points[0] = 0;
    m_points[1] = 0;
    // Top-left
    m_points[2] = 0;
    m_points[3] = height;
    // Bottom-right
    m_points[4] = width;
    m_points[5] = 0;
    // Top-right
    m_points[6] = width;
    m_points[7] = height;
    for (size_t i = 0; i < 4; i++) {
        m_points[i * 2] += x;
        m_points[i * 2 + 1] += y;
    }
}

CharQuadInstanceTracker::CharQuadInstanceTracker(const size_t width, const size_t height)
    : m_quad(0, 0, width, height) {
}

ShapeQuadInfo::ShapeQuadInfo(const GLuint top_left_x, const GLuint top_left_y, const GLuint fill_color,
                             const GLuint border_color, const GLuint pulsing_color, const GLuint shape_id,
                             const GLuint border_thickness, const GLuint width, const GLuint height,
                             const GLfloat rotation_speed, const GLfloat pulsing_speed,
                             const GLfloat pulsing_time_offset, const GLfloat pulsing_time_offset_gradient_x,
                             const GLfloat pulsing_time_offset_gradient_y)
    : m_top_left_x(top_left_x), m_top_left_y(top_left_y), m_width(width), m_height(height), m_fill_color(fill_color),
      m_border_color(border_color), m_pulsing_color(pulsing_color), m_shape_id(shape_id),
      m_border_thickness(border_thickness), m_rotation_speed(rotation_speed), m_pulsing_speed(pulsing_speed),
      m_pulsing_time_offset(pulsing_time_offset), m_pulsing_time_offset_gradient_x(pulsing_time_offset_gradient_x),
      m_pulsing_time_offset_gradient_y(pulsing_time_offset_gradient_y) {
}

EdgeLinePoints::EdgeLinePoints(const std::span<const Vector2<size_t>> points, const GLuint packed_color,
                               const GLuint line_thickness, const GLuint edge_style)
    : m_points{}, m_line_thickness(line_thickness), m_packed_color(packed_color), m_edge_style(edge_style) {
    assert(points.size() >= std::size(m_points));
    for (size_t i = 0; i < std::size(m_points); i++) {
        const auto [x, y] = points[i];
        m_points[i] = Vector2(static_cast<GLuint>(x), static_cast<GLuint>(y));
    }
    if (m_edge_style == edge_style_dotted) {
        // this value is only required with dotted style
        m_total_curve_length = static_cast<GLfloat>(bezierCurveLength(m_points[0].x, m_points[0].y, m_points[1].x,
                                                                      m_points[1].y, m_points[2].x, m_points[2].y,
                                                                      m_points[3].x, m_points[3].y));
    } else {
        m_total_curve_length = 0.0f;
    }
}


EdgeArrowTriangle::EdgeArrowTriangle(const std::span<const Vector2<double>> points, const GLuint packed_color,
                                     const GLuint shape_id)
    : m_points{}, m_shape_id(shape_id), m_packed_color(packed_color) {
    assert(points.size() >= 3);
    for (size_t i = 0; i < 3; i++) {
        const auto [x, y] = points[i];
        m_points[i] = Vector2(static_cast<GLfloat>(x), static_cast<GLfloat>(y));
    }
}

static VAO moveShapeQuadsToBuffer(const std::span<const ShapeQuadInfo> quad_info) {
    GLuint vao, vbo_fake_vertex, vbo_instance;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glGenBuffers(1, &vbo_fake_vertex));
    GL_CHECK(glGenBuffers(1, &vbo_instance));
    GL_CHECK(glBindVertexArray(vao));

    // fake vertex data (1 byte)
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_fake_vertex));
    constexpr GLubyte fake_data[1]{};
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(sizeof(fake_data)), &fake_data, GL_STATIC_DRAW));

    // per-quad info, stuff like quad top left, node shape id, etc.
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_instance));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(quad_info.size() * sizeof(ShapeQuadInfo)),
        quad_info.data(), GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), static_cast<void *>(nullptr)));
    GL_CHECK(glVertexAttribDivisor(0, 1));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(2 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(1, 1));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(4 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(2, 1));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(5 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(3, 1));

    GL_CHECK(glEnableVertexAttribArray(4));
    GL_CHECK(glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(6 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(4, 1));

    GL_CHECK(glEnableVertexAttribArray(5));
    GL_CHECK(glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(7 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(5, 1));

    GL_CHECK(glEnableVertexAttribArray(6));
    GL_CHECK(glVertexAttribIPointer(6, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(8 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(6, 1));

    GL_CHECK(glEnableVertexAttribArray(7));
    GL_CHECK(glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(9 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(7, 1));

    GL_CHECK(glEnableVertexAttribArray(8));
    GL_CHECK(glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(9 * sizeof(GLuint) +
            sizeof(GLfloat))));
    GL_CHECK(glVertexAttribDivisor(8, 1));

    GL_CHECK(glEnableVertexAttribArray(9));
    GL_CHECK(glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(9 * sizeof(GLuint) +
            2 * sizeof(GLfloat))));
    GL_CHECK(glVertexAttribDivisor(9, 1));

    GL_CHECK(glEnableVertexAttribArray(10));
    GL_CHECK(glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(9 * sizeof(GLuint) +
            3 * sizeof(GLfloat))));
    GL_CHECK(glVertexAttribDivisor(10, 1));

    GL_CHECK(glEnableVertexAttribArray(11));
    GL_CHECK(glVertexAttribPointer(11, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(ShapeQuadInfo)), reinterpret_cast<void *>(9 * sizeof(GLuint) +
            4 * sizeof(GLfloat))));
    GL_CHECK(glVertexAttribDivisor(11, 1));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao;
}

static VAO moveEdgeLinesToBuffer(const std::span<const EdgeLinePoints> edge_lines) {
    GLuint vao, vbo;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glGenBuffers(1, &vbo));
    GL_CHECK(glBindVertexArray(vao));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));

    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(edge_lines.size() * sizeof(EdgeLinePoints)),
        edge_lines.data(), GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), static_cast<void *>(nullptr)));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(2 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribPointer(2, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(4 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribPointer(3, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(6 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(4));
    GL_CHECK(glVertexAttribPointer(4, 1, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(8 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(5));
    GL_CHECK(glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(9 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(6));
    GL_CHECK(glVertexAttribIPointer(6, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(10 * sizeof(GLuint))));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao;
}

static VAO moveEdgeSplinesToBuffer(const std::span<const EdgeLinePoints> edge_splines) {
    GLuint vao, vbo_points, vbo_t;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glGenBuffers(1, &vbo_points));
    GL_CHECK(glGenBuffers(1, &vbo_t));
    GL_CHECK(glBindVertexArray(vao));

    // per vertex data (vbo_points) stores all the base points and style attributes
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_points));

    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(edge_splines.size() * sizeof(EdgeLinePoints)),
        edge_splines.data(), GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), static_cast<void *>(nullptr)));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(2 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribPointer(2, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(4 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribPointer(3, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(6 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(4));
    GL_CHECK(glVertexAttribPointer(4, 1, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(8 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(5));
    GL_CHECK(glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(9 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(6));
    GL_CHECK(glVertexAttribIPointer(6, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(10 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(7));
    GL_CHECK(glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(sizeof(EdgeLinePoints)), reinterpret_cast<void *>(11 * sizeof(GLuint))));

    // per instance data is just the `t` value used for bezier interpolation
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_t));

    GLfloat t_values[n_spline_divisions + 1];
    for (size_t i = 0; i <= n_spline_divisions; i++) {
        constexpr auto denom = static_cast<GLfloat>(n_spline_divisions);
        t_values[i] = static_cast<GLfloat>(i) / denom;
    }
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(sizeof(t_values)), t_values, GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(8));
    GL_CHECK(glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(GLfloat)),
        static_cast<void *>(nullptr)));
    GL_CHECK(glVertexAttribDivisor(8, 1));

    // the stride here is not a mistake, attr 9 `t_next` overlaps with the values of attr 8 `t`, just offset by 1
    GL_CHECK(glEnableVertexAttribArray(9));
    GL_CHECK(glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(GLfloat)),
        reinterpret_cast<void *>(sizeof(GLfloat))));
    GL_CHECK(glVertexAttribDivisor(9, 1));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao;
}

static VAO moveArrowTrianglesToBuffer(const std::span<const EdgeArrowTriangle> triangles) {
    GLuint vao, vbo;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glGenBuffers(1, &vbo));
    GL_CHECK(glBindVertexArray(vao));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));

    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(EdgeArrowTriangle), triangles.data(),
        GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeArrowTriangle),
        static_cast<void *>(nullptr)));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeArrowTriangle),
        reinterpret_cast<void *>(2 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeArrowTriangle),
        reinterpret_cast<void *>(4 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(EdgeArrowTriangle),
        reinterpret_cast<void *>(6 * sizeof(GLuint))));

    GL_CHECK(glEnableVertexAttribArray(4));
    GL_CHECK(glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(EdgeArrowTriangle),
        reinterpret_cast<void *>(7 * sizeof(GLuint))));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao;
}

static VAO
moveGLQuadsWithCharQuadPerInstanceDataToBuffer(const std::span<const GLQuad> quads,
                                               const std::span<const CharQuadPerInstanceData> per_instance_data) {
    GLuint vao, vbo_quads, vbo_instances;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glGenBuffers(1, &vbo_quads));
    GL_CHECK(glGenBuffers(1, &vbo_instances));
    GL_CHECK(glBindVertexArray(vao));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_quads));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(quads.size() * sizeof(GLQuad)), quads.data(),
        GL_STATIC_DRAW));
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(2 * sizeof(GLuint)), static_cast<void *>(nullptr)));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_instances));
    GL_CHECK(
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(per_instance_data.size() * sizeof(CharQuadPerInstanceData)),
            per_instance_data.data(), GL_STATIC_DRAW));

    // char_position
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE, 3 * sizeof(GLuint), static_cast<void *>(nullptr)));
    GL_CHECK(glVertexAttribDivisor(1, 1));

    // char_color_packed
    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(GLuint),
        reinterpret_cast<void *>(2 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(2, 1));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao;
}

static void populateRendererCharQuads(const std::vector<GlyphQuad> &quads, const size_t x, const size_t y,
                                      const GLuint font_color,
                                      std::unordered_map<glyph::GlyphCharInfo, CharQuadInstanceTracker,
                                          glyph::GlyphCharInfoHasher> &out_char_quads) {
    // build text quads
    for (const GlyphQuad &quad: quads) {
        if (!out_char_quads.contains(quad.m_c)) {
            out_char_quads.insert_or_assign(
                quad.m_c, CharQuadInstanceTracker(quad.m_right - quad.m_left, quad.m_bottom - quad.m_top));
        }
        CharQuadInstanceTracker &qit = out_char_quads.at(quad.m_c);
        qit.m_instances.emplace_back(
            static_cast<GLuint>(x + quad.m_left),
            static_cast<GLuint>(y + quad.m_top),
            font_color
        );
    }
}

static void parsePulsingTimeOffset(const Attrs &attrs, GLfloat &out, GLfloat &out_grad_x, GLfloat &out_grad_y) {
    constexpr std::string_view attr_name = "punktpulsingtimeoffset";
    const std::string_view str = getAttrOrDefault(attrs, attr_name, "0.0");
    std::string_view values[3]{};
    std::string_view::size_type idx = 0, next_idx = str.find(';');
    for (int i = 0; i < 3; i++) {
        if (next_idx == std::string_view::npos) {
            next_idx = str.length();
        }
        const std::string_view value = str.substr(idx, next_idx - idx);
        values[i] = value;
        idx = next_idx + 1;
        if (idx >= str.length()) {
            break;
        }
        next_idx = str.substr(idx).find(';');
    }

    if (values[0].empty()) {
        out = 0.0f;
    } else {
        out = stringViewToFloat(values[0], attr_name);
    }
    if (values[1].empty()) {
        out_grad_x = 0.0f;
    } else {
        out_grad_x = stringViewToFloat(values[1], attr_name);
    }
    if (values[2].empty()) {
        out_grad_y = 0.0f;
    } else {
        out_grad_y = stringViewToFloat(values[2], attr_name);
    }
}

GLRenderer::GLRenderer(const Digraph &dg, glyph::GlyphLoader &glyph_loader)
    : m_dg(dg), m_glyph_loader(glyph_loader), m_zoom(1.0),
      m_digraph_quad(0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) {
    GLint viewport[4]{};
    GL_CHECK(glGetIntegerv(GL_VIEWPORT, viewport));
    const GLint vw = viewport[2] - viewport[0], vh = viewport[3] - viewport[1];
    m_camera_x = static_cast<double>(static_cast<ssize_t>(dg.m_render_attrs.m_graph_width) - vw) / 2.0;
    m_camera_y = static_cast<double>(static_cast<ssize_t>(dg.m_render_attrs.m_graph_height) - vh) / 2.0;

    // so I don't have to pad my glyph textures
    GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    // background is white
    GL_CHECK(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));

    // enable MSAA for antialiasing
    GL_CHECK(glEnable(GL_MULTISAMPLE));

    // blending for transparency and because my text rendering depends on it
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    // populate m_char_quads, an opengl-friendly quad collection for instanced rendering, and m_node_quads
    constexpr std::string_view font_color_attr = "fontcolor";
    constexpr std::string_view default_font_color = "black";

    // populate the main digraph quad (border and digraph fill color)
    {
        const GLuint font_color = getPackedColorFromAttrs(dg.m_attrs, font_color_attr, default_font_color);
        const GLuint border_color = getPackedColorFromAttrs(dg.m_attrs, "color", "black");
        GLuint fill_color;
        if (dg.m_attrs.contains("fillcolor")) {
            fill_color = getPackedColorFromAttrs(dg.m_attrs, "fillcolor", "white");
        } else if (caseInsensitiveEquals(getAttrOrDefault(dg.m_attrs, "style", "normal"), "filled")) {
            fill_color = border_color;
        } else {
            fill_color = getPackedColorFromName("white");
        }
        const GLuint pulsing_color = getPackedColorFromAttrs(dg.m_attrs, "punktpulsingcolor", "transparent");

        const GLfloat rotation_speed = static_cast<GLfloat>(getAttrTransformedCheckedOrDefault(
            dg.m_attrs, "punktrotationspeed", 0.0f, stringViewToFloat));
        const GLfloat pulsing_speed = static_cast<GLfloat>(getAttrTransformedCheckedOrDefault(
            dg.m_attrs, "punktpulsingspeed", 0.0f, stringViewToFloat));
        GLfloat pulsing_time_offset, pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y;
        parsePulsingTimeOffset(dg.m_attrs, pulsing_time_offset, pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y);

        // build graph quad (for fill and border)
        m_digraph_quad = ShapeQuadInfo(static_cast<GLuint>(dg.m_render_attrs.m_graph_x),
                                       static_cast<GLuint>(dg.m_render_attrs.m_graph_y), fill_color, border_color,
                                       pulsing_color, node_shape_box,
                                       static_cast<GLuint>(dg.m_render_attrs.m_border_thickness),
                                       static_cast<GLuint>(dg.m_render_attrs.m_graph_width),
                                       static_cast<GLuint>(dg.m_render_attrs.m_graph_height), rotation_speed,
                                       pulsing_speed, pulsing_time_offset, pulsing_time_offset_gradient_x,
                                       pulsing_time_offset_gradient_y);

        populateRendererCharQuads(dg.m_render_attrs.m_label_quads, dg.m_render_attrs.m_graph_x,
                                  dg.m_render_attrs.m_graph_y, font_color, m_char_quads);
    }

    // TODO populate cluster quads

    for (const Node &node: std::views::values(dg.m_nodes)) {
        GLuint font_color = getPackedColorFromAttrs(node.m_attrs, font_color_attr, default_font_color);
        const GLuint border_color = getPackedColorFromAttrs(node.m_attrs, "color", "black");
        GLuint fill_color;
        if (node.m_attrs.contains("fillcolor")) {
            fill_color = getPackedColorFromAttrs(node.m_attrs, "fillcolor", "white");
        } else if (caseInsensitiveEquals(getAttrOrDefault(node.m_attrs, "style", "normal"), "filled")) {
            fill_color = border_color;
        } else {
            fill_color = getPackedColorFromName("white");
        }
        const GLuint pulsing_color = getPackedColorFromAttrs(node.m_attrs, "punktpulsingcolor", "transparent");

        const GLfloat rotation_speed = static_cast<GLfloat>(getAttrTransformedCheckedOrDefault(
            node.m_attrs, "punktrotationspeed", 0.0f, stringViewToFloat));
        const GLfloat pulsing_speed = static_cast<GLfloat>(getAttrTransformedCheckedOrDefault(
            node.m_attrs, "punktpulsingspeed", 0.0f, stringViewToFloat));
        GLfloat pulsing_time_offset, pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y;
        parsePulsingTimeOffset(node.m_attrs, pulsing_time_offset, pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y);

        // build node quad (for fill and border)
        m_node_quads.emplace_back(static_cast<GLuint>(node.m_render_attrs.m_x),
                                  static_cast<GLuint>(node.m_render_attrs.m_y), fill_color, border_color, pulsing_color,
                                  getNodeShapeId(node), static_cast<GLuint>(node.m_render_attrs.m_border_thickness),
                                  static_cast<GLuint>(node.m_render_attrs.m_width),
                                  static_cast<GLuint>(node.m_render_attrs.m_height), rotation_speed, pulsing_speed,
                                  pulsing_time_offset, pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y);

        populateRendererCharQuads(node.m_render_attrs.m_quads, node.m_render_attrs.m_x, node.m_render_attrs.m_y,
                                  font_color, m_char_quads);

        // build edge lines (& arrows)
        for (const Edge &edge: node.m_outgoing) {
            if (edge.m_render_attrs.m_trajectory.empty()) {
                // some edges may not have a trajectory because they were replaced by multiple ghost edges
                assert(!edge.m_render_attrs.m_is_visible);
                continue;
            }
            assert(edge.m_render_attrs.m_trajectory.size() == expected_edge_line_length);

            const GLuint edge_color = getPackedColorFromAttrs(edge.m_attrs, "color", "black");
            const GLuint edge_style = getEdgeStyleId(getAttrOrDefault(edge.m_attrs, "style", "solid"));

            const float edge_pen_width = getAttrTransformedCheckedOrDefault(
                edge.m_attrs, "penwidth", 1.0f, stringViewToFloat);
            // TODO handle custom dpi
            constexpr auto dpi = DEFAULT_DPI;
            const auto edge_thickness = static_cast<GLuint>(
                edge_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

            bool render_as_spline = false;
            // only render as spline if that is enabled obviously
            if (edge.m_render_attrs.m_is_spline) {
                const Node &src = dg.m_nodes.at(edge.m_source);
                const Node &dest = dg.m_nodes.at(edge.m_dest);
                const auto source_rank_height = dg.m_render_attrs.m_rank_render_attrs.at(src.m_render_attrs.m_rank).
                        m_rank_height;
                const auto dest_rank_height = dg.m_render_attrs.m_rank_render_attrs.at(dest.m_render_attrs.m_rank).
                        m_rank_height;
                // only render as spline if rendering as a spline makes a difference visually (splines are expensive)
                if (src.m_render_attrs.m_height < source_rank_height ||
                    dest.m_render_attrs.m_height < dest_rank_height || getNodeShapeId(src) != node_shape_box ||
                    getNodeShapeId(dest) != node_shape_box) {
                    render_as_spline = true;
                }
            }

            if (render_as_spline) {
                m_edge_spline_points.emplace_back(edge.m_render_attrs.m_trajectory, edge_color, edge_thickness,
                                                  edge_style);
            } else {
                m_edge_line_points.emplace_back(edge.m_render_attrs.m_trajectory, edge_color, edge_thickness,
                                                edge_style);
            }

            buildArrows(edge, edge_color);

            font_color = getPackedColorFromAttrs(edge.m_attrs, font_color_attr, default_font_color);
            for (const std::vector<GlyphQuad> *quads: {
                     &edge.m_render_attrs.m_label_quads, &edge.m_render_attrs.m_head_label_quads,
                     &edge.m_render_attrs.m_tail_label_quads
                 }) {
                populateRendererCharQuads(*quads, 0, 0, font_color, m_char_quads);
            }
        }
    }

    // populate node quad opengl buffers with node quads
    m_digraph_quad_buffer = moveShapeQuadsToBuffer(std::array<ShapeQuadInfo, 1>({m_digraph_quad}));
    m_cluster_quads_buffer = moveShapeQuadsToBuffer(m_cluster_quads);
    m_node_quad_buffer = moveShapeQuadsToBuffer(m_node_quads);
    m_edge_lines_buffer = moveEdgeLinesToBuffer(m_edge_line_points);
    m_edge_splines_buffer = moveEdgeSplinesToBuffer(m_edge_spline_points);
    m_arrow_triangles_buffer = moveArrowTrianglesToBuffer(m_edge_arrow_triangles);

    // populate glyph vertex and instance buffers
    for (const auto &[key, qit]: m_char_quads) {
        std::array qs{qit.m_quad};
        m_char_buffers.insert_or_assign(key, moveGLQuadsWithCharQuadPerInstanceDataToBuffer(qs, qit.m_instances));
    }

    // unbind buffers so I don't shoot myself in the foot
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // load shaders
    m_nodes_shader = punkt::createShaderProgram(nodes_vertex_shader_code, nodes_geometry_shader_code,
                                         nodes_fragment_shader_code);
    m_chars_shader = punkt::createShaderProgram(chars_vertex_shader_code, chars_geometry_shader_code,
                                         chars_fragment_shader_code);
    m_edges_shader = punkt::createShaderProgram(edges_vertex_shader_code, edges_geometry_shader_code,
                                         edges_fragment_shader_code);
    // note the use of the normal fragment shader also used for straight edge rendering
    m_edge_splines_shader = punkt::createShaderProgram(edges_bezier_vertex_shader_code, edges_bezier_geometry_shader_code,
                                                edges_fragment_shader_code);
    m_arrows_shader = punkt::createShaderProgram(arrows_vertex_shader_code, arrows_geometry_shader_code,
                                          arrows_fragment_shader_code);

    GL_CRITICAL_CHECK_ALL_ERRORS();
}

// determine whether two points for computing the angle of an arrow are too close together and should preferably not
// be used. Policy: line must be longer than arrow.
static bool pointsTooCloseToComputeArrowAngle(const Vector2<size_t> &a, const Vector2<size_t> &b,
                                              const double arrow_size) {
    const auto dx = static_cast<double>(a.x) - static_cast<double>(b.x);
    const auto dy = static_cast<double>(a.y) - static_cast<double>(b.y);
    return dx * dx + dy * dy <= arrow_size * arrow_size;
}

static double getArrowAngle(const std::span<const Vector2<size_t>> trajectory, const bool go_backward,
                            const double arrow_size) {
    const auto &a = go_backward ? trajectory.back() : trajectory.front();

    // the next point in the line (either before or after depending on go_backward) may be the same point duplicated
    // and therefore not usable for computing the angle of the line. Therefore, we have to search the first one that
    // is not equal.
    int start, stop, step;
    if (go_backward) {
        start = static_cast<int>(trajectory.size()) - 2;
        stop = -1;
        step = -1;
    } else {
        start = 1;
        stop = static_cast<int>(trajectory.size());
        step = 1;
    }
    size_t i;
    for (i = start; i != stop; i += step) {
        if (const auto &b = trajectory[i]; !pointsTooCloseToComputeArrowAngle(a, b, arrow_size)) {
            break;
        }
    }
    if (i == stop) {
        // check for exact equivalence: if exactly equal, cannot compute angle -> default to 0. Otherwise, meaning they
        // are just very close together, use anyway as it's the least bad option.
        i += step;
        if (const auto &b = trajectory[i]; a == b) {
            return 0.0;
        }
    }

    // found other reference point for line
    const auto &b = trajectory[i];
    // a and b are swapped in the Y term (first parameter) because y in my coordinate system is down instead of up
    return std::atan2(static_cast<double>(b.y) - static_cast<double>(a.y),
                      static_cast<double>(a.x) - static_cast<double>(b.x));
}

static Vector2<double> getLineEndPointForArrow(const std::span<const Vector2<size_t>> trajectory,
                                               const bool get_min, double &orientation,
                                               const double arrow_size) {
    assert(trajectory.size() == 4);
    if (trajectory.front().y < trajectory.back().y) {
        if (get_min) {
            const auto [x, y] = trajectory.front();
            orientation = getArrowAngle(trajectory, false, arrow_size);
            return Vector2(static_cast<double>(x), static_cast<double>(y));
        } else {
            const auto [x, y] = trajectory.back();
            orientation = getArrowAngle(trajectory, true, arrow_size);
            return Vector2(static_cast<double>(x), static_cast<double>(y));
        }
    } else {
        if (get_min) {
            const auto [x, y] = trajectory.back();
            orientation = getArrowAngle(trajectory, true, arrow_size);
            return Vector2(static_cast<double>(x), static_cast<double>(y));
        } else {
            const auto [x, y] = trajectory.front();
            orientation = getArrowAngle(trajectory, false, arrow_size);
            return Vector2(static_cast<double>(x), static_cast<double>(y));
        }
    }
}

static void buildArrowTriangles(GLRenderer &renderer, const Edge &edge, const GLuint edge_color,
                                const std::string_view arrow_type, const float arrow_size, const bool is_upward) {
    if (arrow_type == "none") {
        return;
    }

    // TODO support more arrow types - for now, every type emits a normal arrow as "fallback"
    Vector2<double> points[3]{};
    // TODO handle custom dpi
    constexpr auto dpi = DEFAULT_DPI;
    const auto arrow_size_pix = arrow_size * arrow_scale * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI);

    double line_orientation, arrow_orientation;
    if (is_upward) {
        auto top = getLineEndPointForArrow(edge.m_render_attrs.m_trajectory, true, line_orientation, arrow_size);
        // since arrows store their coordinates in double precision instead of size_t (i.e. they are not limited to the
        // normal layout grid that other stuff is limited to position-wise), we have to adjust the x so the arrow is
        // centered inside the grid cell it's at.
        top.x += 0.5;
        const auto left = Vector2(top.x - arrow_size_pix / 2, top.y + arrow_size_pix);
        const auto right = Vector2(top.x + arrow_size_pix / 2, top.y + arrow_size_pix);
        points[0] = top;
        points[1] = left;
        points[2] = right;
        arrow_orientation = std::numbers::pi * 0.5;
    } else {
        auto bottom = getLineEndPointForArrow(edge.m_render_attrs.m_trajectory, false, line_orientation, arrow_size);
        bottom.x += 0.5; // for explanation, see ~10 lines above
        const auto left = Vector2(bottom.x - arrow_size_pix / 2, bottom.y - arrow_size_pix);
        const auto right = Vector2(bottom.x + arrow_size_pix / 2, bottom.y - arrow_size_pix);
        points[0] = bottom;
        points[1] = left;
        points[2] = right;
        arrow_orientation = std::numbers::pi * 1.5;
    }

    // rotate the arrow to be aligned with the line
    const double a = line_orientation - arrow_orientation;
    const double sin = std::sin(a), cos = std::cos(a);
    const auto [x_center_i, y_center_i] = points[0];
    const auto x_center = static_cast<double>(x_center_i), y_center = static_cast<double>(y_center_i);
    for (int i = 1; i < 3; i++) {
        auto &p = points[i];
        const auto [xi, yi] = p;
        const auto x = static_cast<double>(xi) - x_center, y = static_cast<double>(yi) - y_center;
        p.x = cos * x + sin * y + x_center;
        p.y = -sin * x + cos * y + y_center;
    }

    renderer.m_edge_arrow_triangles.emplace_back(points, edge_color, 1);
}

void GLRenderer::buildArrows(const Edge &edge, const GLuint edge_color) {
    const Node &src = m_dg.m_nodes.at(edge.m_source);
    const Node &dest = m_dg.m_nodes.at(edge.m_dest);
    const ssize_t rank_diff = static_cast<ssize_t>(dest.m_render_attrs.m_rank)
                              - static_cast<ssize_t>(src.m_render_attrs.m_rank);
    assert(rank_diff == -1 || rank_diff == 1);

    // get relevant attrs
    const std::string_view &arrow_head_type = getAttrOrDefault(edge.m_attrs, "arrowhead", "normal");
    const std::string_view &arrow_tail_type = getAttrOrDefault(edge.m_attrs, "arrowtail", "normal");
    const std::string_view &dir = getAttrOrDefault(edge.m_attrs, "dir", "forward");
    const float arrow_size = getAttrTransformedCheckedOrDefault(edge.m_attrs, "arrowsize", 1.0f, stringViewToFloat);

    const Node *upwards_node{}, *downwards_node{};
    std::string_view upwards_arrow_type, downwards_arrow_type;
    bool has_upwards_arrow{}, has_downwards_arrow{};
    if (rank_diff == -1) {
        upwards_node = &dest;
        downwards_node = &src;
        upwards_arrow_type = arrow_head_type;
        downwards_arrow_type = arrow_tail_type;
        if (dir == "forward") {
            has_upwards_arrow = true;
        } else if (dir == "backward") {
            has_downwards_arrow = true;
        }
    } else {
        upwards_node = &src;
        downwards_node = &dest;
        upwards_arrow_type = arrow_tail_type;
        downwards_arrow_type = arrow_head_type;
        if (dir == "forward") {
            has_downwards_arrow = true;
        } else if (dir == "backward") {
            has_upwards_arrow = true;
        }
    }
    if (dir == "both") {
        has_downwards_arrow = true;
        has_upwards_arrow = true;
    }
    has_upwards_arrow = has_upwards_arrow && !upwards_node->m_render_attrs.m_is_ghost;
    has_downwards_arrow = has_downwards_arrow && !downwards_node->m_render_attrs.m_is_ghost;

    if (has_upwards_arrow) {
        buildArrowTriangles(*this, edge, edge_color, upwards_arrow_type, arrow_size, true);
    }
    if (has_downwards_arrow) {
        buildArrowTriangles(*this, edge, edge_color, downwards_arrow_type, arrow_size, false);
    }
}
