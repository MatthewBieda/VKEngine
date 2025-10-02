#version 450

layout(set = 0, binding = 1) uniform sampler2D tex;

// In GLSL structs must be defined outside the buffer
struct DirectionalLight
{
    vec4 direction;
    vec4 color;
};

struct PointLight
{
    vec4 position;
    vec4 color;
    float radius;
    float padding0;
    float padding1;
    float padding2;
};

layout(std430, set = 0, binding = 2) readonly buffer Lighting
{
    DirectionalLight dirLight;
    int numPointLights;
    PointLight pointLights[16];
} lighting;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	// Debug: visualize normals as colors
	// Take world space normal, normalize to [-1,1], remap to 0->1 and output as RGB
	//outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-lighting.dirLight.direction.xyz); 
    float diff = max(dot(N, L), 0.0);

    vec3 albedo = texture(tex, fragTexCoord).rgb;
    vec3 diffuse = diff * albedo * lighting.dirLight.color.rgb;

    outColor = vec4(diffuse, 1.0);
}
