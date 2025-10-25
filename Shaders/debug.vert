#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
    // ... other fields don't matter for debug rendering
} pc;

void main() {
    gl_Position = pc.proj * pc.view * vec4(inPosition, 1.0);
    fragColor = inColor;
}