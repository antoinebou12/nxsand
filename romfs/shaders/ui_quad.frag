#version 300 es
// Tinted textured quad shader for HUD chips, menu panels, brush ring, font glyphs.
precision highp float;

in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;
uniform int       uMode; // 0 = solid color, 1 = textured, 2 = alpha mask (font)

void main() {
    vec4 base = v_color;
    if (uMode == 1) {
        base *= texture(uTex, v_uv);
    } else if (uMode == 2) {
        float a = texture(uTex, v_uv).r;
        base.a *= a;
    }
    fragColor = base;
}
