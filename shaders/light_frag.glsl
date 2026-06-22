#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} push;

void main() {
    vec2 uv = fragTexCoord - vec2(0.5);
    float dist = length(uv) * 2.0;
    float atten = 1.0 - smoothstep(0.0, 1.0, dist);
    atten *= push.color.a;
    outColor = vec4(push.color.rgb * atten, 1.0);
}
