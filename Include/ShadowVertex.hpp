#pragma once
#include <volk.h>
#include <glm.hpp>

#include <array>

struct ShadowVertex
{
	glm::vec3 pos;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(ShadowVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions;
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(ShadowVertex, pos);
		return attributeDescriptions;
	}
};