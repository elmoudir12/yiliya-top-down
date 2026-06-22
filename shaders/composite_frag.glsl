#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTex;
layout(set = 0, binding = 1) uniform sampler2D lightTex;

void main() {
    vec3 scene = texture(sceneTex, fragTexCoord).rgb;
    vec3 light = texture(lightTex, fragTexCoord).rgb;
    outColor = vec4(scene * light, 1.0);
}
