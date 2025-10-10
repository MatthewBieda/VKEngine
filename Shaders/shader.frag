#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(push_constant) uniform PushConstants
{
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    int enableDirectionalLight;
    int enablePointLights;
} pc;

layout(set = 0, binding = 1) uniform sampler2D tex[];
layout(set = 0, binding = 3) uniform samplerCube skybox;

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
    int padding0;
    int padding1;
    int padding2;
    PointLight pointLights[16];
} lighting;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) flat in uint fragTextureIndex;

layout(location = 0) out vec4 outColor;

// Material properties
const float shininess = 32.0f;
const float specularStrength = 0.5f;

void main() {
	// Debug: visualize normals as colors
	// Take world space normal, normalize to [-1,1], remap to 0->1 and output as RGB
	//outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(pc.cameraPos - fragPos);
    vec3 albedo = texture(nonuniformEXT(tex[fragTextureIndex]), fragTexCoord).rgb;

    vec3 ambient = 0.05 * albedo;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    // Directional light
    if (pc.enableDirectionalLight != 0) 
    {
        vec3 Ldir = normalize(-lighting.dirLight.direction.xyz); 
        vec3 H = normalize(Ldir + V);

        float diff = max(dot(N, Ldir), 0.0);
        diffuse += diff * albedo * lighting.dirLight.color.rgb;

        float specAmount = pow(max(dot(N, H), 0.0), shininess);
        specular += specAmount * specularStrength * lighting.dirLight.color.rgb;
    }

    // Point lights
    if (pc.enablePointLights != 0)
    {
        for (int i = 0; i < lighting.numPointLights; ++i)
        {
            vec3 lightPos = lighting.pointLights[i].position.xyz;
            vec3 Lpoint = lightPos - fragPos;
            float distance = length(Lpoint);
            Lpoint = normalize(Lpoint);

            float attenuation = 1.0 / (distance * distance);

            vec3 Hpoint = normalize(Lpoint + V);

            // Diffuse
            float diffPoint = max(dot(N, Lpoint), 0.0);
            diffuse += diffPoint * albedo * lighting.pointLights[i].color.rgb * attenuation;

            // Specular
            float specPoint = pow(max(dot(N, Hpoint), 0.0), shininess);
            specular += specPoint * specularStrength * lighting.pointLights[i].color.rgb * attenuation;
        };
    }

    // Environment reflections (simple version)
    vec3 R = reflect(-V, N); // Reflect view direction around normal
    vec3 envReflection = texture(skybox, R).rgb;
    
    // Fresnel approximation - objects reflect more at grazing angles
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    
    // Mix in reflections (adjust 0.3 to control reflection strength)
    const float reflectionStrength = 0.3;
    vec3 reflection = envReflection * fresnel * reflectionStrength;
    
    vec3 finalColor = ambient + diffuse + specular + reflection;
    outColor = vec4(finalColor, 1.0);
}
