#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} push;

void main() {
    gl_Position = ubo.projection * ubo.view * push.model * vec4(inPosition, 1.0);
}
