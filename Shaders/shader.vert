#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct Object
{
	mat4 model;
	uint meshIndex;
	uint textureIndex;
	uint padding0;
	uint padding1;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
	Object objects[];
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) flat out uint fragTextureIndex;

layout(push_constant) uniform PushConstants
{
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
} pushConstants;

void main() {
	Object obj = objectData.objects[gl_InstanceIndex];

	mat4 modelMat = obj.model;
	fragTextureIndex = obj.textureIndex;

	vec4 worldPos = modelMat * vec4(inPosition, 1.0);

	fragTexCoord = inTexCoord;
	fragPos = worldPos.xyz;
	fragNormal = mat3(transpose(inverse(modelMat))) * inNormal;

	gl_Position = pushConstants.proj * pushConstants.view * worldPos;
}
