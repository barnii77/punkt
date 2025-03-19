uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;

vec4 unpackColor(uint color) {
    float r = float((color >> 0) & 0xFFU) / 255.0f;
    float g = float((color >> 8) & 0xFFU) / 255.0f;
    float b = float((color >> 16) & 0xFFU) / 255.0f;
    float a = 1.0f;
    return vec4(r, g, b, a);
}

vec2 clockwiseRot90(vec2 v) {
    return vec2(v.y, -v.x);
}
