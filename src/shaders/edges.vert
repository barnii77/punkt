#version 330 core

layout (location = 0) in vec2 vertex0;
layout (location = 1) in vec2 vertex1;
layout (location = 2) in vec2 vertex2;
layout (location = 3) in vec2 vertex3;
layout (location = 4) in float edge_thickness;
layout (location = 5) in uint edge_color_packed;

out VS_OUT {
    vec4 edge_color;
    vec2 vertices[4];
    float edge_thickness;
} vs_out;

vec4 unpackColor(uint color) {
    float r = float((color >> 0) & 0xFFU) / 255.0f;
    float g = float((color >> 8) & 0xFFU) / 255.0f;
    float b = float((color >> 16) & 0xFFU) / 255.0f;
    float a = 1.0f;
    return vec4(r, g, b, a);
}

void main() {
    vs_out.vertices[0] = vertex0;
    vs_out.vertices[1] = vertex1;
    vs_out.vertices[2] = vertex2;
    vs_out.vertices[3] = vertex3;

    vs_out.edge_color = unpackColor(edge_color_packed);
    vs_out.edge_thickness = edge_thickness;

    gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}