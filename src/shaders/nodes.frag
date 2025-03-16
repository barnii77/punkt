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
#define ANTIALIASING_RADIUS 1.5f  // lerp's border and fill color in this amount of pixels

void main() {
    switch (frag_node_shape) {
        case NODE_SHAPE_BOX:
            frag_color = frag_fill_color;
            float border_dist_x = min(frag_node_coord.x, frag_node_size.x - frag_node_coord.x);
            float border_dist_y = min(frag_node_coord.y, frag_node_size.y - frag_node_coord.y);
            float border_dist = min(border_dist_x, border_dist_y);
            if (border_dist <= frag_border_thickness) {
                frag_color = frag_border_color;
            } else if (border_dist - frag_border_thickness < ANTIALIASING_RADIUS) {
                float t = (border_dist - frag_border_thickness) / ANTIALIASING_RADIUS;
                frag_color = frag_color * t + frag_border_color * (1 - t);
            }
            break;
        default:
            frag_color = frag_fill_color;
    }
}
