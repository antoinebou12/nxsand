#version 300 es
precision highp float;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;

void main() {
    vec3 texColor = clamp(texture(uTex, v_uv).rgb, 0.0, 1.0);
    float lum = dot(texColor, vec3(0.2126, 0.7152, 0.0722));
    if (texColor.r > texColor.b + 0.10 && lum > 0.16) {
        fragColor = vec4(texColor * min(lum * 0.40, 0.65), 1.0);
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
