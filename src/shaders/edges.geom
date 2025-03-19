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

    #pragma unroll
    for (int i = 0; i < 4; i++) {
        vec2 vertex = gs_in[0].vertices[i];
        vec2 direction;

        // figure out the "thickness direction", i.e. rot90(line_direction)
        if (i == 0) {
            // The first pair of vertices always has to be emitted because the "skip if vertex equals prev" logic
            // assumes the prev has already been inserted. We therefore search for the next vertex that is not
            // in the same place (and would therefore lead to divide by zero in the normalize call).
            vec2 next_vertex;
            #pragma unroll
            for (int j = 1; j < 4; j++) {
                next_vertex = gs_in[0].vertices[++j];
                if (next_vertex != vertex) {
                    break;
                }
            }
            if (next_vertex == vertex) {
                EndPrimitive();
                return;
            }
            direction = normalize(next_vertex - vertex);
        } else {
            vec2 prev_vertex = gs_in[0].vertices[i - 1];
            // skip if the prev node is in the same location (happens very frequently)
            if (prev_vertex == vertex) {
                continue;
            }
            direction = normalize(vertex - prev_vertex);
        }

        vec2 thickness_direction = clockwiseRot90(direction);

        #pragma unroll
        for (int j = 0; j < 2; j++) {
            vec2 position = vertex + (j == 0 ? thickness_left : thickness_right) * thickness_direction;
            vec2 position_uv = (position - camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;

            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
            frag_edge_color = gs_in[0].edge_color;
            EmitVertex();
        }
    }
    EndPrimitive();
}