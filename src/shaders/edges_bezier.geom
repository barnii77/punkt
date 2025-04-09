#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

#define NVERT 2

in VS_OUT {
    vec4 edge_color;
    vec2 vertices[NVERT + 1];
    float edge_thickness;
    uint edge_style;
    float total_curve_length_pixels;
    float dist_along_edge;
    float next_dist_along_edge;
} gs_in[];

out vec4 frag_edge_color;
out float frag_thickness_position;
out float frag_edge_length_to_thickness_ratio;
// how far along the edge we are as a value in [0, 1]
out float frag_dist_along_edge;
flat out uint frag_edge_style;

vec2 getVertexPosition(vec2 vertex, float thickness_position, float thickness_strength, vec2 thickness_direction) {
    return vertex + thickness_position * thickness_strength * thickness_direction;
}

void emitEdgeVertex(vec2 position, vec2 reverse_transform, float thickness_position,
                    float edge_length_to_thickness_ratio, float dist_along_edge) {
    position = getDirectionTransformedPosition(position);
    vec2 position_uv = (position - reverse_transform * camera_pos) / viewport_size;
    vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
    screen_ndc.y = -screen_ndc.y;

    gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
    frag_edge_color = gs_in[0].edge_color;
    frag_thickness_position = thickness_position;
    frag_edge_length_to_thickness_ratio = edge_length_to_thickness_ratio;
    frag_edge_style = gs_in[0].edge_style;
    frag_dist_along_edge = dist_along_edge;
    EmitVertex();
}

void main() {
    // compute the actual thickness to the left and right. We have to do this floor and ceil business to adhere with
    // the precision that node positions are computed, otherwise we get slight misalignment that looks bad with zoom.
    float thickness_fractional = gs_in[0].edge_thickness / 2.0f;
    float thickness_left = floor(thickness_fractional), thickness_right = ceil(thickness_fractional);
    vec2 reverse_transform = getReverseTransform();

    float dists_along_edge[NVERT];
    dists_along_edge[0] = gs_in[0].dist_along_edge;
    dists_along_edge[1] = gs_in[0].next_dist_along_edge;
    #pragma unroll
    for (int i = 0; i < NVERT; i++) {
        vec2 vertex = gs_in[0].vertices[i];
        vec2 next_vertex = gs_in[0].vertices[i + 1];

        vec2 line_direction = normalize(next_vertex - vertex);
        vec2 thickness_direction;
        if (i == 0 && gs_in[0].dist_along_edge == 0.0f || i == NVERT - 1 && gs_in[0].next_dist_along_edge == 1.0f) {
            thickness_direction = vec2(1.0f, 0.0f);
        } else {
            thickness_direction = clockwiseRot90(line_direction);
        }

        #pragma unroll
        for (int j = 0; j < 2; j++) {
            float thickness_position = j == 0 ? -1.0f : 1.0f;
            vec2 position = getVertexPosition(
                vertex, thickness_position, j == 0 ? thickness_left : thickness_right, thickness_direction);
            emitEdgeVertex(position, reverse_transform, thickness_position,
                           gs_in[0].total_curve_length_pixels / gs_in[0].edge_thickness, dists_along_edge[i]);
        }
    }

    EndPrimitive();
}
