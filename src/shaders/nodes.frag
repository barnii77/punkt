#version 330 core
#include "common.glsl"

in vec4 frag_fill_color;
in vec4 frag_border_color;
in vec2 frag_node_coord;
in vec2 frag_node_size;
flat in uint frag_node_shape;
flat in uint frag_border_thickness;

out vec4 frag_color;

#define NODE_SHAPE_NONE 0u  // default behavior -> handled by default case
#define NODE_SHAPE_BOX 1u
#define NODE_SHAPE_ELLIPSE 2u

#define SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE 10.0f

float smoothLessThan(float a, float b) {
    float x = a - b + 0.5f;
    return smoothstep(1.0f, 0.0f, x * SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE * zoom);
}

// smoothed out version of the hypothetical boolean "is point p inside of the given ellipse?"
float isInEllipse(vec2 p, vec2 radii) {
    vec2 norm = p / radii;
    float d = dot(norm, norm);
    return smoothLessThan(d - 0.5f + 1.0f / SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE, 1.0f);
}

void main() {
    frag_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    switch (frag_node_shape) {
        case NODE_SHAPE_BOX:
            float border_dist_x = min(frag_node_coord.x, frag_node_size.x - frag_node_coord.x);
            float border_dist_y = min(frag_node_coord.y, frag_node_size.y - frag_node_coord.y);
            float border_dist = min(border_dist_x, border_dist_y);
            float on_border = float(frag_border_thickness > 0u) * smoothLessThan(border_dist, frag_border_thickness);
            frag_color = frag_border_color * on_border + frag_fill_color * (1.0f - on_border);
            break;
        case NODE_SHAPE_ELLIPSE:
            vec2 radii = frag_node_size / 2.0f;
            vec2 coords_norm = frag_node_coord - radii;
            float in_outer = isInEllipse(coords_norm, radii);
            float in_inner = isInEllipse(coords_norm, radii - float(frag_border_thickness));
            frag_color = in_inner * frag_fill_color + (in_outer - in_inner) * frag_border_color;
            break;
        default:
            frag_color = frag_fill_color;
    }
}
