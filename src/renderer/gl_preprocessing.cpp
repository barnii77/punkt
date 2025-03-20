#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"
#include "punkt/gl_error.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/int_types.hpp"
#include "generated/punkt/shaders/shaders.hpp"

#include <ranges>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace punkt::render;

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

NodeQuadInfo::NodeQuadInfo(const GLuint top_left_x, const GLuint top_left_y, const GLuint fill_color,
                           const GLuint border_color, const GLuint shape_id, const GLuint border_thickness,
                           const GLuint width,
                           const GLuint height)
    : m_top_left_x(top_left_x), m_top_left_y(top_left_y), m_width(width), m_height(height), m_fill_color(fill_color),
      m_border_color(border_color), m_shape_id(shape_id), m_border_thickness(border_thickness) {
}

EdgeLinePoints::EdgeLinePoints(const std::span<const Vector2<size_t>> points, const GLuint packed_color,
                               const GLuint line_thickness)
    : m_points{}, m_packed_color(packed_color), m_line_thickness(line_thickness) {
    assert(points.size() >= std::size(m_points));
    for (size_t i = 0; i < std::size(m_points); i++) {
        const auto [x, y] = points[i];
        m_points[i] = Vector2(static_cast<GLuint>(x), static_cast<GLuint>(y));
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

static VAO moveNodeQuadsToBuffer(const std::span<const NodeQuadInfo> quad_info) {
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
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(quad_info.size() * sizeof(NodeQuadInfo)),
        quad_info.data(), GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), static_cast<void *>(nullptr)));
    GL_CHECK(glVertexAttribDivisor(0, 1));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), reinterpret_cast<void *>(2 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(1, 1));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), reinterpret_cast<void *>(4 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(2, 1));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), reinterpret_cast<void *>(5 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(3, 1));

    GL_CHECK(glEnableVertexAttribArray(4));
    GL_CHECK(glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), reinterpret_cast<void *>(6 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(4, 1));

    GL_CHECK(glEnableVertexAttribArray(5));
    GL_CHECK(glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT,
        static_cast<GLsizei>(sizeof(NodeQuadInfo)), reinterpret_cast<void *>(7 * sizeof(GLuint))));
    GL_CHECK(glVertexAttribDivisor(5, 1));

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

static void checkShaderCompileStatus(const GLuint shader) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
}

static void checkProgramLinkStatus(const GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << std::endl;
    }
}

static GLuint createShaderProgram(const char *vertex_shader_code, const char *geometry_shader_code,
                                  const char *fragment_shader_code) {
    GLuint vertex_shader = 0;
    if (vertex_shader_code) {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        GL_CHECK(glShaderSource(vertex_shader, 1, &vertex_shader_code, nullptr));
        GL_CHECK(glCompileShader(vertex_shader));
        checkShaderCompileStatus(vertex_shader);
    }

    GLuint geometry_shader = 0;
    if (geometry_shader_code) {
        geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
        GL_CHECK(glShaderSource(geometry_shader, 1, &geometry_shader_code, nullptr));
        GL_CHECK(glCompileShader(geometry_shader));
        checkShaderCompileStatus(geometry_shader);
    }

    GLuint fragment_shader = 0;
    if (fragment_shader_code) {
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        GL_CHECK(glShaderSource(fragment_shader, 1, &fragment_shader_code, nullptr));
        GL_CHECK(glCompileShader(fragment_shader));
        checkShaderCompileStatus(fragment_shader);
    }

    const GLuint program = glCreateProgram();
    if (vertex_shader) {
        GL_CHECK(glAttachShader(program, vertex_shader));
    }
    if (geometry_shader) {
        GL_CHECK(glAttachShader(program, geometry_shader));
    }
    if (fragment_shader) {
        GL_CHECK(glAttachShader(program, fragment_shader));
    }
    GL_CHECK(glLinkProgram(program));
    checkProgramLinkStatus(program);

    GL_CHECK(glDeleteShader(vertex_shader));
    GL_CHECK(glDeleteShader(geometry_shader));
    GL_CHECK(glDeleteShader(fragment_shader));

    return program;
}

static GLuint getPackedColorFromAttrs(const punkt::Attrs &attrs, const std::string_view attr_name,
                                      const std::string_view default_value) {
    const std::string_view &font_color_str = attrs.contains(attr_name) ? attrs.at(attr_name) : default_value;
    uint8_t r, g, b;
    punkt::parseColor(font_color_str, r, g, b);
    return (static_cast<GLuint>(r) << 0) | (static_cast<GLuint>(g) << 8) | (static_cast<GLuint>(b) << 16);
}

// used in the fragment shader for identifying how to draw the border
static GLuint getNodeShapeId(const punkt::Node &node) {
    // TODO handle more shapes
    if (const std::string_view &shape = getAttrOrDefault(node.m_attrs, "shape", default_shape); shape == "none") {
        return 0;
    } else if (shape == "box" || shape == "rect") {
        return 1;
    } else if (shape == "ellipse") {
        return 2;
    } else {
        throw punkt::IllegalAttributeException("shape", std::string(shape));
    }
}

static void populateRendererCharQuads(const std::vector<punkt::GlyphQuad> &quads, const size_t x, const size_t y,
                                      const GLuint font_color,
                                      std::unordered_map<glyph::GlyphCharInfo, CharQuadInstanceTracker,
                                          glyph::GlyphCharInfoHasher> &out_char_quads) {
    // build text quads
    for (const punkt::GlyphQuad &quad: quads) {
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

GLRenderer::GLRenderer(const Digraph &dg, glyph::GlyphLoader &glyph_loader)
    : m_dg(dg), m_glyph_loader(glyph_loader), m_zoom(1.0f) {
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
    for (const Node &node: std::views::values(dg.m_nodes)) {
        constexpr std::string_view font_color_attr = "fontcolor";
        constexpr std::string_view default_font_color = "black";
        GLuint font_color = getPackedColorFromAttrs(node.m_attrs, font_color_attr, default_font_color);
        const GLuint fill_color = getPackedColorFromAttrs(node.m_attrs, "fillcolor", "white");
        const GLuint border_color = getPackedColorFromAttrs(node.m_attrs, "color", "black");

        // build node quad (for fill and border)
        m_node_quads.emplace_back(static_cast<GLuint>(node.m_render_attrs.m_x),
                                  static_cast<GLuint>(node.m_render_attrs.m_y), fill_color, border_color,
                                  getNodeShapeId(node), static_cast<GLuint>(node.m_render_attrs.m_border_thickness),
                                  static_cast<GLuint>(node.m_render_attrs.m_width),
                                  static_cast<GLuint>(node.m_render_attrs.m_height));

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

            const float edge_pen_width = getAttrTransformedCheckedOrDefault(
                edge.m_attrs, "penwidth", 1.0f, stringViewToFloat);
            // TODO handle custom dpi
            constexpr auto dpi = DEFAULT_DPI;
            const auto edge_thickness = static_cast<GLuint>(
                edge_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

            m_edge_line_points.emplace_back(edge.m_render_attrs.m_trajectory, edge_color, edge_thickness);

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
    m_node_quad_buffer = moveNodeQuadsToBuffer(m_node_quads);
    m_edge_lines_buffer = moveEdgeLinesToBuffer(m_edge_line_points);
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
    m_nodes_shader = createShaderProgram(nodes_vertex_shader_code, nodes_geometry_shader_code,
                                         nodes_fragment_shader_code);
    m_chars_shader = createShaderProgram(chars_vertex_shader_code, chars_geometry_shader_code,
                                         chars_fragment_shader_code);
    m_edges_shader = createShaderProgram(edges_vertex_shader_code, edges_geometry_shader_code,
                                         edges_fragment_shader_code);
    m_arrows_shader = createShaderProgram(arrows_vertex_shader_code, arrows_geometry_shader_code,
                                          arrows_fragment_shader_code);

    GL_CRITICAL_CHECK_ALL_ERRORS();
}

// determine whether two points for computing the angle of an arrow are too close together and should preferably not
// be used. Policy: line must be longer than arrow.
static bool pointsTooCloseToComputeArrowAngle(const punkt::Vector2<size_t> &a, const punkt::Vector2<size_t> &b,
                                              const double arrow_size) {
    const auto dx = static_cast<double>(a.x) - static_cast<double>(b.x);
    const auto dy = static_cast<double>(a.y) - static_cast<double>(b.y);
    return dx * dx + dy * dy <= arrow_size * arrow_size;
}

static double getArrowAngle(const std::span<const punkt::Vector2<size_t>> trajectory, const bool go_backward,
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

static punkt::Vector2<double> getLineEndPointForArrow(const std::span<const punkt::Vector2<size_t>> trajectory,
                                                      const bool get_min, double &orientation,
                                                      const double arrow_size) {
    assert(trajectory.size() == 4);
    if (trajectory.front().y < trajectory.back().y) {
        if (get_min) {
            const auto [x, y] = trajectory.front();
            orientation = getArrowAngle(trajectory, false, arrow_size);
            return punkt::Vector2(static_cast<double>(x), static_cast<double>(y));
        } else {
            const auto [x, y] = trajectory.back();
            orientation = getArrowAngle(trajectory, true, arrow_size);
            return punkt::Vector2(static_cast<double>(x), static_cast<double>(y));
        }
    } else {
        if (get_min) {
            const auto [x, y] = trajectory.back();
            orientation = getArrowAngle(trajectory, true, arrow_size);
            return punkt::Vector2(static_cast<double>(x), static_cast<double>(y));
        } else {
            const auto [x, y] = trajectory.front();
            orientation = getArrowAngle(trajectory, false, arrow_size);
            return punkt::Vector2(static_cast<double>(x), static_cast<double>(y));
        }
    }
}

static void buildArrowTriangles(GLRenderer &renderer, const punkt::Edge &edge, const GLuint edge_color,
                                const std::string_view arrow_type, const float arrow_size, const bool is_upward) {
    if (arrow_type == "none") {
        return;
    }

    // TODO support more arrow types - for now, every type emits a normal arrow as "fallback"
    punkt::Vector2<double> points[3]{};
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
        const auto left = punkt::Vector2(top.x - arrow_size_pix / 2, top.y + arrow_size_pix);
        const auto right = punkt::Vector2(top.x + arrow_size_pix / 2, top.y + arrow_size_pix);
        points[0] = top;
        points[1] = left;
        points[2] = right;
        arrow_orientation = std::numbers::pi * 0.5;
    } else {
        auto bottom = getLineEndPointForArrow(edge.m_render_attrs.m_trajectory, false, line_orientation, arrow_size);
        bottom.x += 0.5; // for explanation, see ~10 lines above
        const auto left = punkt::Vector2(bottom.x - arrow_size_pix / 2, bottom.y - arrow_size_pix);
        const auto right = punkt::Vector2(bottom.x + arrow_size_pix / 2, bottom.y - arrow_size_pix);
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
