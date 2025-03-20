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

#define IN_ELLIPSE_ANTIALIASING_INTERPOLATION_START 0.97f

// smoothed out version of the hypothetical boolean "is point p inside of the given ellipse?"
float isInEllipse(vec2 p, vec2 radii) {
    vec2 norm = p / radii;
    float d = dot(norm, norm);
    float scale = 1.0f / (1.0f - IN_ELLIPSE_ANTIALIASING_INTERPOLATION_START);
    float x = (d - IN_ELLIPSE_ANTIALIASING_INTERPOLATION_START) * scale;
    return smoothstep(1.0f, 0.0f, x);
}

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
            float in_outer = isInEllipse(coords_norm, radii);
            float in_inner = isInEllipse(coords_norm, radii - float(frag_border_thickness));
            frag_color = in_inner * frag_fill_color + (in_outer - in_inner) * frag_border_color;
            break;
        default:
            frag_color = frag_fill_color;
    }
}
