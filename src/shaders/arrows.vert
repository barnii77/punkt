#version 330 core
#include "common.glsl"

layout (location = 0) in vec2 vertex0;
layout (location = 1) in vec2 vertex1;
layout (location = 2) in vec2 vertex2;
layout (location = 3) in uint shape_id;
layout (location = 4) in uint packed_color;

out VS_OUT {
    vec4 color;
    vec2 vertices[3];
    uint shape_id;
} vs_out;

void main() {
    vs_out.vertices[0] = vertex0;
    vs_out.vertices[1] = vertex1;
    vs_out.vertices[2] = vertex2;
    vs_out.color = unpackColor(packed_color);
    vs_out.shape_id = shape_id;
}