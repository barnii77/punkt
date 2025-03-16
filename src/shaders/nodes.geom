#version 330 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT {
    vec4 geom_fill_color;
    vec4 geom_border_color;
    vec2 geom_node_top_left;
    vec2 geom_node_size;
    flat uint geom_node_shape;
    flat uint geom_border_thickness;
} gs_in[];

out vec4 frag_fill_color;
out vec4 frag_border_color;
out vec2 frag_node_coord;
out vec2 frag_node_size;
flat out uint frag_node_shape;
flat out uint frag_border_thickness;

uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;

void main() {
    #pragma unroll
    for (int i = 0; i < 2; i++) {
        #pragma unroll
        for (int j = 0; j < 2; j++) {
            vec2 vertex_position = gs_in[0].geom_node_top_left + gs_in[0].geom_node_size * vec2(i, j);
            vec2 position_uv = (vertex_position - camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;
            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);

            frag_fill_color = gs_in[0].geom_fill_color;
            frag_border_color = gs_in[0].geom_border_color;

            frag_node_coord = vertex_position - gs_in[0].geom_node_top_left;
            frag_node_shape = gs_in[0].geom_node_shape;
            frag_border_thickness = gs_in[0].geom_border_thickness;
            frag_node_size = gs_in[0].geom_node_size;

            EmitVertex();
        }
    }
    EndPrimitive();
}
