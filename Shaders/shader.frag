#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 6) uniform CascadeBuffer
{
    mat4 cascadeViewProjs[4];
    vec4 cascadeSplits;
} cascadeData;

layout(push_constant) uniform PushConstants
{
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    int enableDirectionalLight;
    int enablePointLights;
    int enableAlphaTest;
    int diffuseTextureIndex;
    int normalTextureIndex;
    int enableNormalMaps;
    float reflectionStrength;
    int showCascadeColors;
} pc;

layout(set = 0, binding = 1) uniform sampler2D tex[];
layout(set = 0, binding = 3) uniform samplerCube skybox;
layout(set = 0, binding = 5) uniform sampler2DShadow shadowMaps[4];

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
    PointLight pointLights[128];
} lighting;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal; // Original World Space normal
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent; // World space tangent
layout(location = 4) in vec3 fragBitangent; // World space bitangent
layout(location = 5) in vec4 fragLightSpacePos[4]; // One per cascade

layout(location = 0) out vec4 outColor;

// Material constants
const float shininess = 32.0f;
const float specularStrength = 0.1f;
const int NO_TEXTURE = -1;

// Select cascade based on view-space depth
int SelectCascade(float viewDepth)
{
    // Compare against cascade split distances
    if (viewDepth < cascadeData.cascadeSplits.x) return 0;
    if (viewDepth < cascadeData.cascadeSplits.y) return 1;
    if (viewDepth < cascadeData.cascadeSplits.z) return 2;
    return 3;
}

float SampleCascade(int cascadeIndex, vec4 lightSpacePos)
{
    // Perspective Divide (convert clip space to normalized device coordinates (NDC))
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Transform to [0, 1] range (texture coordinates)
    // Only transform X and Y to the [0, 1] range for texture sampling
    // Z is preserved as the NDC depth value
    projCoords = vec3(projCoords.xy * 0.5 + 0.5, projCoords.z);

    // Return sentinel if outside the cascade frustum
    if (projCoords.z > 1.0 || projCoords.z < 0.0)
        return -1.0;

    // Current fragment depth (Z-value in light space)
    float currentDepth = projCoords.z;
    float shadow = 0.0;

    // Apply PCF
    const int kernelSize = 1;
    float texelSize = 1.0 / 2048.0;

    for (int x = -kernelSize; x <= kernelSize; ++x)
    {
        for (int y = -kernelSize; y <= kernelSize; ++y)
        {
            // Offset the sample point by a fraction of the texel size
            vec2 offset = vec2(float(x), float(y)) * texelSize;

            // The hardware sampler compares 'currentDepth' against the depth map 
            // and returns 1.0 (lit) or 0.0 (shadowed).
            // The result is stored in 'shadow' without manual comparison.
            shadow += texture(shadowMaps[cascadeIndex], vec3(projCoords.xy + offset, currentDepth));
        }
    }

    // Average the results
    float numSamples = float((2 * kernelSize + 1) * (2 * kernelSize + 1));
    return shadow / numSamples;
}

float ShadowCalculation(vec4 lightSpacePos[4], float viewDepth)
{
    int cascadeIndex = SelectCascade(viewDepth);

    float shadow = SampleCascade(cascadeIndex, lightSpacePos[cascadeIndex]);

    // If outside current cascade, try the next one
    if (shadow < 0.0 && cascadeIndex < 3)
    {
        shadow = SampleCascade(cascadeIndex + 1, lightSpacePos[cascadeIndex + 1]);
    }

    // If still outside (last cascade), treat as fully lit
    if (shadow < 0.0)
        shadow = 1.0;

    return shadow;
}

void main() {
	// Debug: visualize normals as colors
	// Take world space normal, normalize to [-1,1], remap to 0->1 and output as RGB
	//outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

    vec3 N = normalize(fragNormal);

    if (pc.normalTextureIndex != NO_TEXTURE && pc.enableNormalMaps != 0)
    {
        vec3 T = normalize(fragTangent);
        vec3 B = normalize(fragBitangent);
        mat3 TBN = mat3(T, B, N);

        vec4 normalSample = texture(nonuniformEXT(tex[pc.normalTextureIndex]), fragTexCoord);

        // Transform the sampled RGB [0, 1] into a normal vector in tangent space [-1, 1]
        vec3 sampledNormal = normalize(normalSample.rgb * 2.0 - 1.0);

        N = normalize(TBN * sampledNormal);
    }

    vec3 V = normalize(pc.cameraPos - fragPos);

    // Sample color and alpha
    vec3 albedo = vec3(1.0, 0.0, 1.0); // Pink signals missing texture
    float alpha = 1.0; // Default to opaque

    if (pc.diffuseTextureIndex != NO_TEXTURE)
    {
        vec4 texSample = texture(nonuniformEXT(tex[pc.diffuseTextureIndex]), fragTexCoord);
        albedo = texSample.rgb;
        alpha = texSample.a;
    }

    // Alpha test
    if (pc.enableAlphaTest != 0 && alpha < 0.8)
    {
        discard;
    }

    vec3 ambient = 0.05 * albedo;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    // Calculate the view-space depth for cascade selection
    vec4 viewPos = pc.view * vec4(fragPos, 1.0);
    float viewDepth = abs(viewPos.z);

    // Cacade Visualization Demo
    // Visualize the cascade *actually used for sampling* and the resulting shadow factor.
    // This shows cascade color (R/G/B/Y) modulated by the sampled shadow (1.0 = lit, 0.0 = shadowed).
    if (pc.showCascadeColors != 0)
    {
        float viewDepth = abs(viewPos.z);
        int cascadeIndex = SelectCascade(viewDepth);

        vec3 cascadeColor;
        if (cascadeIndex == 0)
        {
            cascadeColor = vec3(1.0, 0.0, 0.0);
        }
        else if (cascadeIndex == 1)
        {
            cascadeColor = vec3(0.0, 1.0, 0.0);
        }
        else if (cascadeIndex == 2)
        {
            cascadeColor = vec3(0.0, 0.0, 1.0);
        }
        else
        {
            cascadeColor = vec3(1.0, 1.0, 0.0);
        }

        float shadowFactor = ShadowCalculation(fragLightSpacePos, viewDepth);

        outColor = vec4(cascadeColor * shadowFactor, 1.0);
        return;
    }

    float shadowFactor = ShadowCalculation(fragLightSpacePos, viewDepth);

    // Directional light
    if (pc.enableDirectionalLight != 0) 
    {
        vec3 Ldir = normalize(-lighting.dirLight.direction.xyz); 
        vec3 H = normalize(Ldir + V);

        float diff = max(dot(N, Ldir), 0.0);
        diffuse += diff * albedo * lighting.dirLight.color.rgb * shadowFactor;

        float specAmount = pow(max(dot(N, H), 0.0), shininess);
        specular += specAmount * specularStrength * lighting.dirLight.color.rgb * shadowFactor;
    }

    // Point lights
    if (pc.enablePointLights != 0)
    {
        for (int i = 0; i < lighting.numPointLights; ++i)
        {
            vec3 lightPos = lighting.pointLights[i].position.xyz;
            vec3 Lpoint = lightPos - fragPos;
            float dist = length(Lpoint);

            if (dist > lighting.pointLights[i].radius) continue;
            float attenuation = 1.0 / (dist * dist);

            // This is way slower for some reason
            // float attenuation = clamp(1.0 - dist / lighting.pointLights[i].radius, 0.0, 1.0);
            // attenuation *= attenuation;

            Lpoint = normalize(Lpoint);
            vec3 Hpoint = normalize(Lpoint + V);

            // Diffuse
            float diffPoint = max(dot(N, Lpoint), 0.0);
            diffuse += diffPoint * albedo * lighting.pointLights[i].color.rgb * attenuation;

            // Specular
            float specPoint = pow(max(dot(N, Hpoint), 0.0), shininess);
            specular += specPoint * specularStrength * lighting.pointLights[i].color.rgb * attenuation;
        };
    }

    // Only apply reflections if reflectionStrength > 0
    vec3 reflection = vec3(0.0);

    if (pc.reflectionStrength > 0.0)
    {
        // Environment reflections
        vec3 R = reflect(-V, N); // Reflect view direction around normal
        vec3 envReflection = texture(skybox, R).rgb;

        // Fresnel approximation - objects reflect more at grazing angles
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);

        reflection = envReflection * fresnel * pc.reflectionStrength;
    }
    
    vec3 finalColor = ambient + diffuse + specular + reflection;
    outColor = vec4(finalColor, pc.enableAlphaTest != 0 ? 1.0 : alpha);
}
