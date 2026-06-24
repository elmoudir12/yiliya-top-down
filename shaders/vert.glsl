#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec4 lightPos;
    vec4 lightColor;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} push;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPos;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
    fragColor = push.color;
}
