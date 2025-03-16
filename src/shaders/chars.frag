#version 330 core

in vec2 frag_glyph_uv;
in vec4 frag_glyph_color;

out vec4 frag_color;

uniform sampler2D font_texture;

void main() {
    float alpha = texture(font_texture, frag_glyph_uv).r;
    frag_color = vec4(frag_glyph_color.rgb, alpha);
}
