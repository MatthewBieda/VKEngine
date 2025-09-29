#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 proj;
} ubo;

layout(set = 0, binding = 1) readonly buffer ObjectBuffer
{
	mat4 model[];
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out uint objectIndex;

void main() {
	uint instanceIndex = gl_InstanceIndex;
	objectIndex = instanceIndex;
	mat4 modelMat = objectData.model[instanceIndex];
	gl_Position = ubo.proj * ubo.view * modelMat * vec4(inPosition, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}
