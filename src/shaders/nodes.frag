#version 330 core
#include "common.glsl"

in vec4 frag_fill_color;
in vec4 frag_border_color;
in vec4 frag_pulsing_color;
in vec2 frag_node_coord;
in vec2 frag_node_size;
in float frag_pulsing_speed;
in float frag_pulsing_time_offset;
flat in uint frag_node_shape;
flat in uint frag_border_thickness;

out vec4 frag_color;

#define NODE_SHAPE_NONE 0u  // default behavior -> handled by default case
#define NODE_SHAPE_BOX 1u
#define NODE_SHAPE_ELLIPSE 2u

vec4 applySpecialEffects(vec4 color) {
    float strength = max(sin(time * frag_pulsing_speed + 2 * PI * frag_pulsing_time_offset), 0.0f);
    return mix(color, frag_pulsing_color, strength);
}

void main() {
    frag_color = vec4(0.0f);
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
    frag_color = applySpecialEffects(frag_color);
}
