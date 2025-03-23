#version 330 core
#include "common.glsl"

#define EDGE_STYLE_SOLID 0u
#define EDGE_STYLE_DOTTED 1u
#define EDGE_STYLE_DASHED 2u
#define EDGE_STYLE_BULLET 3u

// decides how many segments (meaning separate dots or dashes) there are along a styled (non-solid) line
#define N_SEGMENTS_PER_STYLED_EDGE 20.0f

in vec4 frag_edge_color;
in float frag_thickness_position;
in float frag_edge_length_to_thickness_ratio;
// how far along the edge we are as a value in [0, 1]
in float frag_dist_along_edge;
flat in uint frag_edge_style;

out vec4 frag_color;

void main() {
    uint segment;
    float in_segment_pos, d, segment_scale_dist_along_edge;
    vec2 rel_xy;
    switch (frag_edge_style) {
        case EDGE_STYLE_DOTTED:
            segment_scale_dist_along_edge = frag_dist_along_edge * frag_edge_length_to_thickness_ratio;
            segment = uint(floor(segment_scale_dist_along_edge));
            in_segment_pos = fract(segment_scale_dist_along_edge);
            rel_xy = vec2(frag_thickness_position, 2.0f * in_segment_pos - 1.0f);
            d = sqrt(dot(rel_xy, rel_xy));
            frag_color = uint(segment) % 2u == 0u ? frag_edge_color : vec4(0.0f);
            frag_color *= smoothLessThan(d - 0.5f + 1.0f / SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE, 1.0f);
            break;
        case EDGE_STYLE_DASHED:
            segment = uint(floor(frag_dist_along_edge * N_SEGMENTS_PER_STYLED_EDGE));
            frag_color = uint(segment) % 2u == 0u ? frag_edge_color : vec4(0.0f);
            break;
        case EDGE_STYLE_BULLET:
            segment_scale_dist_along_edge = frag_dist_along_edge * N_SEGMENTS_PER_STYLED_EDGE;
            segment = uint(floor(segment_scale_dist_along_edge));
            in_segment_pos = fract(segment_scale_dist_along_edge);
            rel_xy = vec2(frag_thickness_position, 2.0f * in_segment_pos - 1.0f);
            d = dot(rel_xy, rel_xy);
            frag_color = uint(segment) % 2u == 0u ? frag_edge_color : vec4(0.0f);
            frag_color *= smoothLessThan(d - 0.5f + 1.0f / SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE, 1.0f);
            break;
        default: // solid is default
            frag_color = frag_edge_color;
    }
}
