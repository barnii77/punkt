#version 330 core
#include "common.glsl"

layout (location = 0) in vec2 vertex0;
layout (location = 1) in vec2 vertex1;
layout (location = 2) in vec2 vertex2;
layout (location = 3) in vec2 vertex3;
layout (location = 4) in float edge_thickness;
layout (location = 5) in uint edge_color_packed;
layout (location = 6) in uint edge_style;
layout (location = 7) in float total_curve_length_pixels;
layout (location = 8) in float t;
layout (location = 9) in float next_t;

#define NVERT 2

out VS_OUT {
    vec4 edge_color;
    vec2 vertices[NVERT + 1];
    float edge_thickness;
    uint edge_style;
    float total_curve_length_pixels;
    float dist_along_edge;
    float next_dist_along_edge;
} vs_out;

vec2 evaluateBezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    float u = 1.0f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

void main() {
    vec2 vertex_a = evaluateBezier(vertex0, vertex1, vertex2, vertex3, t);
    vec2 vertex_b = evaluateBezier(vertex0, vertex1, vertex2, vertex3, next_t);
    float next_next_t = 2.0f * next_t - t;
    vec2 vertex_c = evaluateBezier(vertex0, vertex1, vertex2, vertex3, next_next_t);
    vs_out.vertices[0] = vertex_a;
    vs_out.vertices[1] = vertex_b;
    // Vertex c is the end point vertex of the next segment. This is required for computing the thickness direction.
    vs_out.vertices[2] = vertex_c;

    vs_out.edge_color = unpackColor(edge_color_packed);
    vs_out.edge_thickness = edge_thickness;
    vs_out.edge_style = edge_style;
    vs_out.total_curve_length_pixels = total_curve_length_pixels;
    vs_out.dist_along_edge = t;
    vs_out.next_dist_along_edge = next_t;
}
