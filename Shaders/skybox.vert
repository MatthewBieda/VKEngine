#version 450

layout(location = 0) out vec3 fragTexCoord;

// Cube vertices (no vertex buffer needed)
vec3 positions[36] = vec3[](
    // Front
    vec3(-1, -1,  1), vec3( 1, -1,  1), vec3( 1,  1,  1),
    vec3( 1,  1,  1), vec3(-1,  1,  1), vec3(-1, -1,  1),
    // Back
    vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3(-1,  1, -1), vec3( 1,  1, -1), vec3( 1, -1, -1),
    // Left
    vec3(-1, -1, -1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3(-1,  1,  1), vec3(-1,  1, -1), vec3(-1, -1, -1),
    // Right
    vec3( 1, -1,  1), vec3( 1, -1, -1), vec3( 1,  1, -1),
    vec3( 1,  1, -1), vec3( 1,  1,  1), vec3( 1, -1,  1),
    // Top
    vec3(-1,  1,  1), vec3( 1,  1,  1), vec3( 1,  1, -1),
    vec3( 1,  1, -1), vec3(-1,  1, -1), vec3(-1,  1,  1),
    // Bottom
    vec3(-1, -1, -1), vec3( 1, -1, -1), vec3( 1, -1,  1),
    vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, -1, -1)
);

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 proj;
} pc;

void main() {
    vec3 pos = positions[gl_VertexIndex];
    
    // Remove translation from view matrix
    mat4 viewNoTranslation = mat4(mat3(pc.view));
    
    vec4 clipPos = pc.proj * viewNoTranslation * vec4(pos, 1.0);
    
    // Set depth to max (1.0) for skybox
    gl_Position = clipPos.xyww;
    
    fragTexCoord = pos;
}