#version 330 core

// vertex data
layout (location = 0) in vec2 quad_vertex;
// instance data
layout (location = 1) in vec2 char_position;
layout (location = 2) in uint char_color_packed;

out vec2 frag_glyph_uv;
out vec4 frag_glyph_color;

uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;
uniform uint font_size;

vec4 unpackColor(uint color) {
    float r = float((color >> 0) & 0xFFU) / 255.0f;
    float g = float((color >> 8) & 0xFFU) / 255.0f;
    float b = float((color >> 16) & 0xFFU) / 255.0f;
    float a = 1.0f;
    return vec4(r, g, b, a);
}

void main() {
    // quad_vertex is a general quad fit for the font size, while char_position is the position of the glyph instance
    vec2 position = quad_vertex + char_position;
    // normalize position with camera and viewport and map to NDC
    vec2 position_uv = (position - camera_pos) / viewport_size;
    vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
    // flip y dimension because any sane UI library has (0, 0) in the top left corner
    screen_ndc.y = -screen_ndc.y;

    gl_Position = vec4(screen_ndc, 0.0f, 1.0f);

    frag_glyph_uv = quad_vertex / vec2(font_size, font_size);
    frag_glyph_color = unpackColor(char_color_packed);
}
