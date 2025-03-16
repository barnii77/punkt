#version 330 core

layout (points) in;
layout (triangle_strip, max_vertices = 8) out;

in VS_OUT {
    vec4 edge_color;
    vec2 vertices[4];
    float edge_thickness;
} gs_in[];

out vec4 frag_edge_color;

uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;

vec2 clockwiseRot90(vec2 v) {
    return vec2(v.y, -v.x);
}

void main() {
//    // TODO remove
//    vec2 position = gs_in[0].vertices[0] + 1. * gs_in[0].edge_thickness / 2.0f * vec2(1., 0.);
//    vec2 position_uv = (position - camera_pos) / viewport_size;
//    vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
//    screen_ndc.y = -screen_ndc.y;
//
//    frag_edge_color = gs_in[0].edge_color;
//    gl_Position = vec4(screen_ndc * 20.0f, 0.0f, 1.0f);
//    vec2 vertex = gs_in[0].vertices[0];
//    frag_edge_color = vec4(gs_in[0].vertices[2] == gs_in[0].vertices[2 + 1], 0., 0., 1.0);
//    gl_Position = vec4(-.5, -.5, 0., 1.);
//    EmitVertex();
//
//    //    vertex = gs_in[0].vertices[1];
//    //    frag_edge_color = vec4(0.0f, 0.0f, 0.0f, 1.0);
//    //    gl_Position = vec4(-.5, .5, 0., 1.);
//    //    EmitVertex();
//    //
//    //    vertex = gs_in[0].vertices[2];
//    //    frag_edge_color = vec4(0.0f, 0.0f, 0.0f, 1.0);
//    //    gl_Position = vec4(.5, -.5, 0., 1.);
//    //    EmitVertex();
//    int i = 1;
//    vec2 thickness_direction = vec2(1., 0.);
//
//    #pragma unroll
//    int c = 0;
//    for (float j = -1.0f; j < 2.0f; j += 2.0f) {
//        if (c++ == 2) break;
//        //        j = -1. * c;
//        vec2 position = vertex + j * gs_in[0].edge_thickness / 2.0f * thickness_direction;
//        vec2 position_uv = (position - camera_pos) / viewport_size;
//        vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
//        screen_ndc.y = -screen_ndc.y;
//
//        frag_edge_color = gs_in[0].edge_color;
//        gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
//        EmitVertex();
//    }
//
//    i = 2;
//    vertex = gs_in[0].vertices[i];
//    vec2 direction;
//    if (i == 0) {
//        vec2 next_vertex = gs_in[0].vertices[1];
//        if (next_vertex == vertex) {
//            return;
//        }
//        direction = normalize(next_vertex - vertex);
//    } else {
//        vec2 prev_vertex = gs_in[0].vertices[i - 1];
//        if (prev_vertex == vertex) {
//            return;
//        }
//        direction = normalize(vertex - prev_vertex);
//    }
//    thickness_direction = clockwiseRot90(direction);
//
//    #pragma unroll
//    c = 0;
//    for (float j = -1.0f; j < 2.0f; j += 2.0f) {
//        if (c++ == 2) break;
//        //        j = -1. * c;
//        vec2 position = vertex + j * gs_in[0].edge_thickness / 2.0f * thickness_direction;
//        vec2 position_uv = (position - camera_pos) / viewport_size;
//        vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
//        screen_ndc.y = -screen_ndc.y;
//
//        frag_edge_color = gs_in[0].edge_color;
//        gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
//        EmitVertex();
//    }
//    return;

    // TODO remove everything above
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
        for (float j = -1.0f; j < 2.0f; j += 2.0f) {
            vec2 position = vertex + j * gs_in[0].edge_thickness / 2.0f * thickness_direction;
            vec2 position_uv = (position - camera_pos) / viewport_size;
            vec2 screen_ndc = zoom * (2.0f * position_uv - 1.0f);
            screen_ndc.y = -screen_ndc.y;

            frag_edge_color = gs_in[0].edge_color;
            gl_Position = vec4(screen_ndc, 0.0f, 1.0f);
            EmitVertex();
        }
    }
    EndPrimitive();
}