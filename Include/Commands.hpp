#pragma once

#include "volk.h"
#include <vector>

class VulkanContext;

class Commands 
{
public:
	Commands(VulkanContext& context, uint32_t maxFramesInFlight);
	~Commands();

	VkCommandPool getCommandPool() const { return m_commandPool; }
	VkCommandBuffer getCommandBuffer(uint32_t frameIndex) const { return m_commandBuffers[frameIndex]; }

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
	VulkanContext& m_context;
	uint32_t m_maxFramesInFlight;

	VkCommandPool m_commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_commandBuffers;
};