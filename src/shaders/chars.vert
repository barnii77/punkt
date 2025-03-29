#version 330 core
#include "common.glsl"

// vertex data
layout (location = 0) in vec2 quad_vertex;
// instance data
layout (location = 1) in vec2 char_position;
layout (location = 2) in uint char_color_packed;

out vec2 frag_glyph_uv;
out vec4 frag_glyph_color;

uniform uint font_size;

void main() {
    // quad_vertex is a general quad fit for the font size, while char_position is the position of the glyph instance
    vec2 position = getDirectionTransformedPosition(quad_vertex + char_position);
    vec2 reverse_transform = getReverseTransform();

    // normalize position with camera and viewport and map to NDC
    vec2 position_uv = (position - reverse_transform * camera_pos) / viewport_size;
    vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
    // flip y dimension because any sane 2D graphics library has (0, 0) in the top left corner
    screen_ndc.y = -screen_ndc.y;

    gl_Position = vec4(screen_ndc, 0.0f, 1.0f);

    frag_glyph_uv = getDirectionTransformedPosition(quad_vertex) / vec2(font_size);
    frag_glyph_uv = applyReverseTranformToUV(frag_glyph_uv, reverse_transform);
    frag_glyph_color = unpackColor(char_color_packed);
}
