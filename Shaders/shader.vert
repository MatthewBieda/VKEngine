#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct Object
{
	mat4 model;
	uint meshIndex;
	uint isVisible;
	uint padding2;
	uint padding3;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
	Object objects[];
} objectData;

layout(set = 0, binding = 4) readonly buffer VisibleIndexData
{
	// This array holds the global index of the instance to draw
	uint visibleIndices[];
} visibleIndexData;

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
} pc;

void main() {
	// Get the local index of the instance being drawn
	uint filteredInstanceIndex = gl_InstanceIndex;

	// Use the local index to look up the true global index in the Visible Index buffer
	// This fetches the sparse global index
	uint globalIndex = visibleIndexData.visibleIndices[filteredInstanceIndex];

	// Use the true global index to fetch the correct unique instance data
	Object obj = objectData.objects[globalIndex];

	mat4 modelMat = obj.model;
	vec4 worldPos = modelMat * vec4(inPosition, 1.0);

	fragTexCoord = inTexCoord;
	fragPos = worldPos.xyz;
	fragNormal = mat3(transpose(inverse(modelMat))) * inNormal;

	gl_Position = pc.proj * pc.view * worldPos;
}
