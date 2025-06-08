#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT {
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
} gs_in[];

out vec4 frag_fill_color;
out vec4 frag_border_color;
out vec4 frag_pulsing_color;
out vec2 frag_node_coord;
out vec2 frag_node_size;
out float frag_pulsing_speed;
out float frag_pulsing_time_offset;
flat out uint frag_node_shape;
flat out uint frag_border_thickness;

vec2 applyRotation(vec2 v, vec2 center) {
    float sine = sin(time * gs_in[0].roation_speed);
    float cosine = cos(time * gs_in[0].roation_speed);
    mat2 rotation_matrix = mat2(cosine, sine, -sine, cosine);
    v *= rotation_matrix;
    return v;
}

void main() {
    vec2 reverse_transform = getReverseTransform();
    vec2 center = gs_in[0].node_top_left + gs_in[0].node_size / 2.0f;

    #pragma unroll
    for (int i = 0; i < 2; i++) {
        #pragma unroll
        for (int j = 0; j < 2; j++) {
            vec2 vertex_position = gs_in[0].node_top_left + gs_in[0].node_size * vec2(i, j);
            vec2 dist_from_center = vertex_position - center;
            vertex_position = center + applyRotation(dist_from_center, center);
            vec2 dir_transformed_vertex = getDirectionTransformedPosition(vertex_position);
            vec2 position_uv = (dir_transformed_vertex - reverse_transform * camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;
            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);

            frag_fill_color = gs_in[0].fill_color;
            frag_border_color = gs_in[0].border_color;
            frag_pulsing_color = gs_in[0].pulsing_color;

            frag_node_coord = vertex_position - gs_in[0].node_top_left;
            frag_node_shape = gs_in[0].node_shape;
            frag_border_thickness = gs_in[0].border_thickness;
            frag_node_size = gs_in[0].node_size;

            frag_pulsing_speed = gs_in[0].pulsing_speed;
            vec2 grad = gs_in[0].pulsing_time_offset_gradient * vec2(i, j);
            float grad_sum = grad.x + grad.y;
            frag_pulsing_time_offset = gs_in[0].pulsing_time_offset + grad_sum;

            EmitVertex();
        }
    }
    EndPrimitive();
}
