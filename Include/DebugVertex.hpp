#pragma once
#include <volk.h>
#include <glm.hpp>

#include <array>

struct DebugVertex
{
	glm::vec3 pos;
	glm::vec4 color;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(DebugVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription() 
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(DebugVertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(DebugVertex, color);

        return attributeDescriptions;
    }
};