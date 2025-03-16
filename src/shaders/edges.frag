#version 330 core

in vec4 frag_edge_color;

out vec4 frag_color;

void main() {
    frag_color = frag_edge_color;
}