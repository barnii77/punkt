#version 330 core

layout (location = 0) in vec2 vertex0;
layout (location = 1) in vec2 vertex1;
layout (location = 2) in vec2 vertex2;
layout (location = 3) in float t;
layout (location = 4) in float next_t;

out VS_OUT {
    vec2 vertices[2];
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

    vs_out.dist_along_edge = t;
    vs_out.next_dist_along_edge = next_t;
}
