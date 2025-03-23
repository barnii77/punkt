uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;
uniform int is_sideways;
uniform int is_reversed;

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

vec2 getDirectionTransformedPosition(vec2 position) {
    return is_sideways != 0 ? position.yx : position;
}

vec2 getReverseTransform() {
    vec2 reverse_transform = is_reversed != 0 ? vec2(1.0f, -1.0f) : vec2(1.0f);
    if (is_sideways != 0) {
        reverse_transform = reverse_transform.yx;
    }
    return reverse_transform;
}

vec2 applyReverseTranformToUV(vec2 uv, vec2 transform) {
    return (transform * (2.0f * uv - 1.0f) + 1.0f) / 2.0f;
}
