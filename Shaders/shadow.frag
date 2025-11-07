#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 1) uniform sampler2D tex[];

layout(push_constant) uniform PushConstants
{
	mat4 lightViewProj;
	uint enableAlphaTest;
	uint diffuseTextureIndex;
} pc;

layout(location = 0) in vec2 fragTexCoord; 

void main()
{
	if (pc.enableAlphaTest != 0)
	{
        float alpha = texture(tex[nonuniformEXT(pc.diffuseTextureIndex)], fragTexCoord).a;

		if (alpha < 0.8)
		{
			discard;
		}
	}
}
