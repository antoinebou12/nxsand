#version 300 es
precision highp float;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;
uniform sampler2D uBlurTex;
uniform float     uBloomScalar;
uniform float     uExposure;
uniform float     uGamma;
uniform float     uSaturation;

void main() {
    vec3 hdr = max(vec3(0.0), texture(uTex, v_uv).rgb);
    vec3 bloom = texture(uBlurTex, v_uv).rgb;
    hdr += bloom * uBloomScalar;
    vec3 result = vec3(1.0) - exp(-hdr * uExposure);
    result = pow(result, vec3(1.0 / uGamma));
    float lum = dot(result, vec3(0.2, 0.7, 0.1));
    vec3 diff = result - vec3(lum);
    fragColor = vec4(diff * uSaturation + lum, 1.0);
}
