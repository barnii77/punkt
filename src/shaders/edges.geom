#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 8) out;

in VS_OUT {
    vec4 edge_color;
    vec2 vertices[4];
    float edge_thickness;
} gs_in[];

out vec4 frag_edge_color;

void main() {
    // compute the actual thickness to the left and right. We have to do this floor and ceil business to adhere with
    // the precision that node positions are computed, otherwise we get slight misalignment that looks bad with zoom.
    float thickness_fractional = gs_in[0].edge_thickness / 2.0f;
    float thickness_left = floor(thickness_fractional), thickness_right = ceil(thickness_fractional);
    vec2 reverse_transform = getReverseTransform();

    #pragma unroll
    for (int i = 0; i < 4; i++) {
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
            vec2 position = getDirectionTransformedPosition(
                vertex + (j == 0 ? -thickness_left : thickness_right) * thickness_direction);
            vec2 position_uv = (position - reverse_transform * camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * reverse_transform * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;

            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
            frag_edge_color = gs_in[0].edge_color;
            EmitVertex();
        }
    }
    EndPrimitive();
}