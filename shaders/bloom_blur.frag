#version 300 es
precision highp float;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2      uTexSize;
uniform vec2      uDir;

void main() {
    vec2 texel = 1.0 / uTexSize;
    vec2 stride = texel * uDir;
    fragColor = texture(uTex, v_uv) * 0.0093;
    fragColor += texture(uTex, v_uv + stride * 1.0) * 0.028002;
    fragColor += texture(uTex, v_uv - stride * 1.0) * 0.028002;
    fragColor += texture(uTex, v_uv + stride * 2.0) * 0.065984;
    fragColor += texture(uTex, v_uv - stride * 2.0) * 0.065984;
    fragColor += texture(uTex, v_uv + stride * 3.0) * 0.121703;
    fragColor += texture(uTex, v_uv - stride * 3.0) * 0.121703;
    fragColor += texture(uTex, v_uv + stride * 4.0) * 0.175713;
    fragColor += texture(uTex, v_uv - stride * 4.0) * 0.175713;
    fragColor += texture(uTex, v_uv + stride * 5.0) * 0.198596;
    fragColor += texture(uTex, v_uv - stride * 5.0) * 0.198596;
    fragColor += texture(uTex, v_uv + stride * 6.0) * 0.175713;
    fragColor += texture(uTex, v_uv - stride * 6.0) * 0.175713;
    fragColor += texture(uTex, v_uv + stride * 7.0) * 0.121703;
    fragColor += texture(uTex, v_uv - stride * 7.0) * 0.121703;
    fragColor += texture(uTex, v_uv + stride * 8.0) * 0.065984;
    fragColor += texture(uTex, v_uv - stride * 8.0) * 0.065984;
    fragColor += texture(uTex, v_uv + stride * 9.0) * 0.028002;
    fragColor += texture(uTex, v_uv - stride * 9.0) * 0.028002;
    fragColor += texture(uTex, v_uv + stride * 10.0) * 0.0093;
    fragColor += texture(uTex, v_uv - stride * 10.0) * 0.0093;
}
