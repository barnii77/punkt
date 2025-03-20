#version 330 core

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

void main() {
    frag_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    switch (frag_node_shape) {
        case NODE_SHAPE_BOX:
            frag_color = frag_fill_color;
            float border_dist_x = min(frag_node_coord.x, frag_node_size.x - frag_node_coord.x);
            float border_dist_y = min(frag_node_coord.y, frag_node_size.y - frag_node_coord.y);
            float border_dist = min(border_dist_x, border_dist_y);
            if (border_dist <= frag_border_thickness) {
                frag_color = frag_border_color;
            }
            break;
        case NODE_SHAPE_ELLIPSE:
            vec2 radii = frag_node_size / 2.0f;
            vec2 coords_norm = frag_node_coord - radii;
            vec2 border_intersection = radii * normalize(coords_norm / radii);
            vec2 center = vec2(0.0f, 0.0f);
            float intersection_center_dist = distance(border_intersection, center);
            float coords_center_dist = distance(coords_norm, center);
            float dist = intersection_center_dist - coords_center_dist;
            if (0.0f <= dist && dist <= frag_border_thickness) {
                frag_color = frag_border_color;
            } else if (0.0f <= dist) {
                frag_color = frag_fill_color;
            }
            break;
        default:
            frag_color = frag_fill_color;
    }
}
