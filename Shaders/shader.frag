#version 450

layout(set = 0, binding = 1) uniform sampler2D tex;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	// Debug: visualize normals as colors
	// Take world space normal, normalize to [-1,1], remap to 0->1 and output as RGB
	outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

	//outColor = texture(tex, fragTexCoord);
}
