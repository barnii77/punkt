#version 330 core
#include "common.glsl"

// per quad data
layout (location = 0) in vec2 node_top_left;
layout (location = 1) in vec2 node_size;
layout (location = 2) in uint fill_color_packed;
layout (location = 3) in uint border_color_packed;
layout (location = 4) in uint pulsing_color_packed;
layout (location = 5) in uint node_shape;
layout (location = 6) in uint border_thickness;
layout (location = 7) in float rotation_speed;
layout (location = 8) in float pulsing_speed;
layout (location = 9) in float pulsing_time_offset;
layout (location = 10) in float pulsing_time_offset_gradient_x;
layout (location = 11) in float pulsing_time_offset_gradient_y;

out VS_OUT {
    vec4 fill_color;
    vec4 border_color;
    vec4 pulsing_color;
    vec2 node_top_left;
    vec2 node_size;
    vec2 pulsing_time_offset_gradient;
    float roation_speed;
    float pulsing_speed;
    float pulsing_time_offset;
    flat uint node_shape;
    flat uint border_thickness;
} vs_out;

void main() {
    vs_out.fill_color = unpackColor(fill_color_packed);
    vs_out.border_color = unpackColor(border_color_packed);
    vs_out.pulsing_color = unpackColor(pulsing_color_packed);

    vs_out.node_top_left = node_top_left;
    vs_out.node_shape = node_shape;
    vs_out.border_thickness = border_thickness;
    vs_out.node_size = node_size;
    vs_out.roation_speed = rotation_speed;
    vs_out.pulsing_speed = pulsing_speed;
    vs_out.pulsing_time_offset = pulsing_time_offset;
    vs_out.pulsing_time_offset_gradient = vec2(pulsing_time_offset_gradient_x, pulsing_time_offset_gradient_y);
}
