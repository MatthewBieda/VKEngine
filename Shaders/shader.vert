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

layout(set = 0, binding = 6) uniform CascadeBuffer
{
	mat4 cascadeViewProjs[4];
	vec4 cascadeSplits;
} cascadeData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent; // Tangent vector (includes handedness in .w)

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal; // World space normal
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragTangent; // World space tangent
layout(location = 4) out vec3 fragBitangent; // World space bitangent
layout(location = 5) out vec4 fragLightSpacePos[4]; // light space fragment positions

// Can only declare a subset which we need
layout(push_constant) uniform PushConstants
{
	mat4 view;
	mat4 proj;
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

	// Calculate the position in the light's clip space for all cascades
	for (int i = 0; i < 4; ++i)
	{
		fragLightSpacePos[i] = cascadeData.cascadeViewProjs[i] * worldPos;	
	}

	// Matrix for transforming Normals/Tangents
	mat3 normalMat = mat3(transpose(inverse(modelMat)));

	// Transform N, T to World Space
	vec3 N = normalize(normalMat * inNormal);
	vec3 T = normalize(normalMat * inTangent.xyz);

	// Optional: Re-orthogonalize T to N to correct for non-uniform scaling/skewing 
    // This makes sure N and T are strictly perpendicular in world space.
	T = normalize(T - dot(T, N) * N);

	// Calculate Bitangent
	vec3 B = normalize(cross(N, T) * inTangent.w);

	// Output World Space vectors
	fragPos = worldPos.xyz;
	fragNormal = N;
	fragTexCoord = inTexCoord;
	fragTangent = T;
	fragBitangent = B;

	gl_Position = pc.proj * pc.view * worldPos;
}
