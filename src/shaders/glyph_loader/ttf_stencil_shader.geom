#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangles, max_vertices = 12) out;

in VS_OUT {
    vec2 vertices[2];
    float dist_along_edge;
    float next_dist_along_edge;
} gs_in[];

void main() {
    vec4 vertex_a = vec4(gs_in[0].vertices[0], 0.0, 0.0);
    vec4 vertex_b = vec4(gs_in[0].vertices[1], 0.0, 0.0);
    if (vertex_a.y > vertex_b.y || vertex_a.y == vertex_b.y && vertex_a.x > vertex_b.x) {
        vec4 tmp = vertex_a;
        vertex_a = vertex_b;
        vertex_b = tmp;
    }

    // left (back-facing) triangles
    gl_Position = vertex_a; EmitVertex();
    gl_Position = vec4(-1.0, vertex_a.yzw); EmitVertex();
    gl_Position = vec4(vertex_b); EmitVertex();
    // second triangle of left
    gl_Position = vertex_b; EmitVertex();
    gl_Position = vec4(-1.0, vertex_a.yzw); EmitVertex();
    gl_Position = vec4(-1.0, vertex_b.yzw); EmitVertex();
    EndPrimitive();

    // right (front-facing) triangles
    gl_Position = vertex_a; EmitVertex();
    gl_Position = vec4(1.0, vertex_a.yzw); EmitVertex();
    gl_Position = vec4(vertex_b); EmitVertex();
    // second triangle of left
    gl_Position = vertex_b; EmitVertex();
    gl_Position = vec4(1.0, vertex_a.yzw); EmitVertex();
    gl_Position = vec4(1.0, vertex_b.yzw); EmitVertex();
    EndPrimitive();
}
