#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
	mat4 model[];
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

layout(push_constant) uniform PushConstants
{
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} pushConstants;

void main() {
	mat4 modelMat = objectData.model[gl_InstanceIndex];
	vec4 worldPos = modelMat * vec4(inPosition, 1.0);

	fragTexCoord = inTexCoord;
	fragPos = worldPos.xyz;
	fragNormal = mat3(transpose(inverse(modelMat))) * inNormal;

	gl_Position = pushConstants.proj * pushConstants.view * worldPos;
}
