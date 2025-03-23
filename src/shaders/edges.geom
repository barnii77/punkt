#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 8) out;

#define NVERT 4

in VS_OUT {
    vec4 edge_color;
    vec2 vertices[NVERT];
    float edge_thickness;
    uint edge_style;
} gs_in[];

out vec4 frag_edge_color;
out float frag_thickness_position;
out float frag_edge_length_to_thickness_ratio;
// how far along the edge we are as a value in [0, 1]
out float frag_dist_along_edge;
flat out uint frag_edge_style;

void main() {
    // compute the actual thickness to the left and right. We have to do this floor and ceil business to adhere with
    // the precision that node positions are computed, otherwise we get slight misalignment that looks bad with zoom.
    float thickness_fractional = gs_in[0].edge_thickness / 2.0f;
    float thickness_left = floor(thickness_fractional), thickness_right = ceil(thickness_fractional);
    vec2 reverse_transform = getReverseTransform();

    float dists_along_edge[NVERT];
    dists_along_edge[0] = 0.0f;
    // compute a prefix sum of distances between vertices
    #pragma unroll
    for (int i = 1; i < NVERT; i++) {
        dists_along_edge[i] = dists_along_edge[i - 1] + distance(gs_in[0].vertices[i], gs_in[0].vertices[i - 1]);
    }
    float edge_length_to_thickness_ratio = dists_along_edge[NVERT - 1] / gs_in[0].edge_thickness;
    // normalize the prefix sum results into range [0, 1]
    #pragma unroll
    for (int i = 0; i < NVERT; i++) {
        dists_along_edge[i] /= dists_along_edge[NVERT - 1];
    }

    #pragma unroll
    for (int i = 0; i < NVERT; i++) {
        // figure out the "thickness direction", i.e. rot90(line_direction)
        vec2 line_direction = normalize(gs_in[0].vertices[2] - gs_in[0].vertices[1]);
        vec2 thickness_direction = clockwiseRot90(line_direction);
        if (i == 0 || i == 3) {
            // the first and last point are straight down lines, only [1] -> [2] is sideways
            thickness_direction.y = 0.0f;
        }

        vec2 vertex = gs_in[0].vertices[i];

        #pragma unroll
        for (int j = 0; j < 2; j++) {
            float thickness_position = j == 0 ? -1.0f : 1.0f;
            vec2 position = getDirectionTransformedPosition(
                vertex + thickness_position * (j == 0 ? thickness_left : thickness_right) * thickness_direction);
            vec2 position_uv = (position - reverse_transform * camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;

            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
            frag_edge_color = gs_in[0].edge_color;
            frag_thickness_position = thickness_position;
            frag_edge_length_to_thickness_ratio = edge_length_to_thickness_ratio;
            frag_edge_style = gs_in[0].edge_style;
            frag_dist_along_edge = dists_along_edge[i];
            EmitVertex();
        }
    }
    EndPrimitive();
}
