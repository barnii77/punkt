#version 330 core
#include "common.glsl"

layout (location = 0) in vec2 vertex0;
layout (location = 1) in vec2 vertex1;
layout (location = 2) in vec2 vertex2;
layout (location = 3) in vec2 vertex3;
layout (location = 4) in float edge_thickness;
layout (location = 5) in uint edge_color_packed;
layout (location = 6) in uint edge_style;

out VS_OUT {
    vec4 edge_color;
    vec2 vertices[4];
    float edge_thickness;
    uint edge_style;
} vs_out;

void main() {
    vs_out.vertices[0] = vertex0;
    vs_out.vertices[1] = vertex1;
    vs_out.vertices[2] = vertex2;
    vs_out.vertices[3] = vertex3;

    vs_out.edge_color = unpackColor(edge_color_packed);
    vs_out.edge_thickness = edge_thickness;
    vs_out.edge_style = edge_style;
}