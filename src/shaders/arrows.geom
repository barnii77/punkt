#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec4 color;
    vec2 vertices[3];
    uint shape_id;
} gs_in[];

out vec4 frag_arrow_color;
flat out uint frag_shape_id;

void main() {
    vec2 reverse_transform = getReverseTransform();
    #pragma unroll
    for (int i = 0; i < 3; i++) {
        vec2 vertex_position = getDirectionTransformedPosition(gs_in[0].vertices[i]);
        vec2 position_uv = (vertex_position - reverse_transform * camera_pos) / viewport_size;
        vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
        screen_ndc.y = -screen_ndc.y;

        gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
        frag_arrow_color = gs_in[0].color;
        frag_shape_id = gs_in[0].shape_id;

        EmitVertex();
    }
    EndPrimitive();
}
