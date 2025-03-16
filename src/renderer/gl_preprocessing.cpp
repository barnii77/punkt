#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"
#include "punkt/gl_error.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "generated/punkt/shaders/shaders.hpp"

#include <ranges>
#include <array>
#include <cassert>

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
    : m_top_left_x(top_left_x), m_top_left_y(top_left_y), m_fill_color(fill_color), m_border_color(border_color),
      m_shape_id(shape_id), m_border_thickness(border_thickness), m_width(width), m_height(height) {
}

EdgeLinePoints::EdgeLinePoints(const std::span<const Vector2<size_t>> points, const GLuint packed_color,
                               const GLuint line_thickness)
    : m_points{}, m_packed_color(packed_color), m_line_thickness(line_thickness) {
    assert(points.size() >= std::size(m_points));
    for (size_t i = 0; i < std::size(m_points); i++) {
        const auto &[x, y] = points[i];
        m_points[i] = Vector2(static_cast<GLuint>(x), static_cast<GLuint>(y));
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

static GLuint getNodeShapeId(const punkt::Node &node) {
    // TODO handle padding for non-rectangular node shapes (ellipse, oval)
    if (const std::string_view &shape = node.m_attrs.contains("shape") ? node.m_attrs.at("shape") : default_shape;
        shape == "none") {
        return 0;
    } else if (shape == "box" || shape == "rect") {
        return 1;
    } else {
        throw punkt::IllegalAttributeException("shape", std::string(shape));
    }
}

GLuint GLRenderer::getGlyphTexture(const glyph::GlyphCharInfo &gci) {
    if (m_glyph_textures.contains(gci)) {
        return m_glyph_textures[gci];
    }
    const glyph::Glyph &glyph = m_glyph_loader.getGlyph(gci.c, gci.font_size);
    GLuint texture;
    GL_CHECK(glGenTextures(1, &texture));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, static_cast<GLsizei>(glyph.m_width),
        static_cast<GLsizei>(glyph.m_height), 0, GL_RED, GL_UNSIGNED_BYTE, glyph.m_texture.data()));
    GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D)); // for zooming out
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    m_glyph_textures.insert_or_assign(gci, texture);
    return texture;
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

    // populate m_char_quads, an opengl-friendly quad collection for instanced rendering, and m_node_quads
    for (const Node &node: std::views::values(dg.m_nodes)) {
        const GLuint font_color = getPackedColorFromAttrs(node.m_attrs, "fontcolor", "black");
        const GLuint fill_color = getPackedColorFromAttrs(node.m_attrs, "fillcolor", "white");
        const GLuint border_color = getPackedColorFromAttrs(node.m_attrs, "color", "black");

        // build node quad (for fill and border)
        m_node_quads.emplace_back(static_cast<GLuint>(node.m_render_attrs.m_x),
                                  static_cast<GLuint>(node.m_render_attrs.m_y), fill_color, border_color,
                                  getNodeShapeId(node), static_cast<GLuint>(node.m_render_attrs.m_border_thickness),
                                  static_cast<GLuint>(node.m_render_attrs.m_width),
                                  static_cast<GLuint>(node.m_render_attrs.m_height));

        // build text quads
        for (const GlyphQuad &quad: node.m_render_attrs.m_quads) {
            if (!m_char_quads.contains(quad.m_c)) {
                m_char_quads.insert_or_assign(
                    quad.m_c, CharQuadInstanceTracker(quad.m_right - quad.m_left, quad.m_bottom - quad.m_top));
            }
            CharQuadInstanceTracker &qit = m_char_quads.at(quad.m_c);
            qit.m_instances.emplace_back(
                static_cast<GLuint>(node.m_render_attrs.m_x + quad.m_left),
                static_cast<GLuint>(node.m_render_attrs.m_y + quad.m_top),
                font_color
            );
        }

        // build edge lines
        for (const Edge &edge: node.m_outgoing) {
            if (edge.m_render_attrs.m_trajectory.empty()) {
                // some edges may not have a trajectory because they were replaced by multiple ghost edges
                assert(!edge.m_render_attrs.m_is_visible);
                continue;
            }
            assert(edge.m_render_attrs.m_trajectory.size() == expected_edge_line_length);

            const GLuint edge_color = getPackedColorFromAttrs(edge.m_attrs, "color", "black");

            const float edge_pen_width = edge.m_attrs.contains("penwidth")
                                             ? stringViewToFloat(edge.m_attrs.at("penwidth"), "penwidth")
                                             : 1.0f;
            // TODO handle custom dpi
            constexpr auto dpi = DEFAULT_DPI;
            const auto edge_thickness = static_cast<GLuint>(
                edge_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

            m_edge_line_points.emplace_back(edge.m_render_attrs.m_trajectory, edge_color, edge_thickness);
        }
    }

    // populate node quad opengl buffers with node quads
    m_node_quad_buffer = moveNodeQuadsToBuffer(m_node_quads);
    m_edge_lines_buffer = moveEdgeLinesToBuffer(m_edge_line_points);

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

    GL_CRITICAL_CHECK_ALL_ERRORS();
}
