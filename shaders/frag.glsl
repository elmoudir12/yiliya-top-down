#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec4 lightPos;
    vec4 lightColor;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    if (texColor.a < 0.01) discard;

    float dist = length(ubo.lightPos.xyz - fragWorldPos);
    float radius = ubo.lightPos.w;
    float ambient = ubo.lightColor.a;
    float intensity = ambient + (1.0 - ambient) * max(0.0, 1.0 - dist / radius);
    intensity = clamp(intensity, ambient, 1.0);

    outColor = vec4(texColor.rgb * intensity, texColor.a);
}
