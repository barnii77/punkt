#version 330 core
#include "common.glsl"

// per quad data
layout (location = 0) in vec2 node_top_left;
layout (location = 1) in vec2 node_size;
layout (location = 2) in uint fill_color_packed;
layout (location = 3) in uint border_color_packed;
layout (location = 4) in uint node_shape;
layout (location = 5) in uint border_thickness;

out VS_OUT {
    vec4 fill_color;
    vec4 border_color;
    vec2 node_top_left;
    vec2 node_size;
    flat uint node_shape;
    flat uint border_thickness;
} vs_out;

void main() {
    vs_out.fill_color = unpackColor(fill_color_packed);
    vs_out.border_color = unpackColor(border_color_packed);

    vs_out.node_top_left = node_top_left;
    vs_out.node_shape = node_shape;
    vs_out.border_thickness = border_thickness;
    vs_out.node_size = node_size;
}
