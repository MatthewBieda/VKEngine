#version 450

layout (location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants
{
	mat4 lightViewProj; // light's view-projection matrix
} pc;

void main()
{
	// Transform vertex position into light's clip space
	gl_Position = pc.lightViewProj * vec4(inPosition, 1.0);
}