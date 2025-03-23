uniform vec2 viewport_size;
uniform vec2 camera_pos;
uniform float zoom;
uniform int is_sideways;
uniform int is_reversed;

#define SMOOTH_LT_ANTIALIASING_INTERPOLATION_SCALE 10.0f

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
