#version 330 core

// per quad data
layout (location = 0) in vec2 node_top_left;
layout (location = 1) in vec2 node_size;
layout (location = 2) in uint fill_color_packed;
layout (location = 3) in uint border_color_packed;
layout (location = 4) in uint node_shape;
layout (location = 5) in uint border_thickness;

out VS_OUT {
    vec4 geom_fill_color;
    vec4 geom_border_color;
    vec2 geom_node_top_left;
    vec2 geom_node_size;
    flat uint geom_node_shape;
    flat uint geom_border_thickness;
} vs_out;

vec4 unpackColor(uint color) {
    float r = float((color >> 0) & 0xFFU) / 255.0f;
    float g = float((color >> 8) & 0xFFU) / 255.0f;
    float b = float((color >> 16) & 0xFFU) / 255.0f;
    float a = 1.0f;
    return vec4(r, g, b, a);
}

void main() {
    gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    vs_out.geom_fill_color = unpackColor(fill_color_packed);
    vs_out.geom_border_color = unpackColor(border_color_packed);

    vs_out.geom_node_top_left = node_top_left;
    vs_out.geom_node_shape = node_shape;
    vs_out.geom_border_thickness = border_thickness;
    vs_out.geom_node_size = node_size;
}
