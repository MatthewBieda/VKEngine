#version 450

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

layout(push_constant) uniform PushConstants
{
	mat4 lightViewProj; // light's view-projection matrix
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent; // Tangent vector (includes handedness in .w)

layout(location = 0) out vec2 fragTexCoord;

void main()
{
	// 0: Pass through texture coords
	fragTexCoord = inTexCoord;

	// 1. Get the local index of the instance being drawn
	uint filteredInstanceIndex = gl_InstanceIndex;

	// 2. Use the local index to look up the true global index
	uint globalIndex = visibleIndexData.visibleIndices[filteredInstanceIndex];

	// 3. Fetch the object's Model Matrix
	Object obj = objectData.objects[globalIndex];
	mat4 modelMat = obj.model;

	// 4. Transform vertex position from Model -> World -> Light Clip Space
	vec4 worldPos = modelMat * vec4(inPosition, 1.0);
	gl_Position = pc.lightViewProj * worldPos;
}
