#version 330 core
#include "common.glsl"

layout (points) in;
layout (triangle_strip, max_vertices = 10) out;

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

// emits the start and end segments that bring the edge onto the height/width where the main line segment can be drawn
vec2 emitSideSegment(vec2 line_direction, float thickness_left, float thickness_right, vec2 reverse_transform,
                     float edge_length_to_thickness_ratio, float dists_along_edge[NVERT], int i_start, int i_stop) {
    vec2 thickness_direction = vec2(1.0f, 0.0f);
    int start, stop, jstep;
    if (line_direction.x < 0.0f) {
        start = 1;
        stop = -1;
        jstep = -1;
    } else {
        start = 0;
        stop = 2;
        jstep = 1;
    }
    vec2 last_emitted_vertex = vec2(0.0f);
    for (int i = i_start; i < i_stop; i++) {
        vec2 vertex = gs_in[0].vertices[i];
        for (int j = start; j != stop; j += jstep) {
            float thickness_position = j == 0 ? -1.0f : 1.0f;
            float thickness_strength = j == 0 ? thickness_left : thickness_right;
            vec2 position = getVertexPosition(vertex, thickness_position, thickness_strength, thickness_direction);
            emitEdgeVertex(
                position, reverse_transform, thickness_position, edge_length_to_thickness_ratio, dists_along_edge[i]);
            last_emitted_vertex = position;
        }
    }
    return last_emitted_vertex;
}

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

    vec2 line_direction = normalize(gs_in[0].vertices[2] - gs_in[0].vertices[1]);

    // draw the line out from the source node
    vec2 last_emitted_vertex = emitSideSegment(line_direction, thickness_left, thickness_right, reverse_transform,
                                               edge_length_to_thickness_ratio, dists_along_edge, 0, 2);

    vec2 thickness_direction = clockwiseRot90(line_direction);
    // emit the extra vertex for cleaner line rotation
    vec2 position = last_emitted_vertex + (line_direction.x < 0.0f ? 1.0f : -1.0f) * thickness_direction;
    emitEdgeVertex(position, reverse_transform, 1.0f, edge_length_to_thickness_ratio, edge_length_to_thickness_ratio);

    // emit the extra vertex on the other side
    vec2 next_emitted_vertex, next_thickness_direction = vec2(1.0f, 0.0f);
    if (line_direction.x < 0.0f) {
        next_emitted_vertex = getVertexPosition(gs_in[0].vertices[2], 1.0f, thickness_right, next_thickness_direction);
    } else {
        next_emitted_vertex = getVertexPosition(gs_in[0].vertices[2], -1.0f, thickness_left, next_thickness_direction);
    }
    position = next_emitted_vertex + (line_direction.x < 0.0f ? -1.0f : 1.0f) * thickness_direction;
    emitEdgeVertex(position, reverse_transform, 1.0f, edge_length_to_thickness_ratio, edge_length_to_thickness_ratio);

    // draw the line into the dest node
    emitSideSegment(line_direction, thickness_left, thickness_right, reverse_transform, edge_length_to_thickness_ratio,
                    dists_along_edge, 2, NVERT);
    EndPrimitive();
}
