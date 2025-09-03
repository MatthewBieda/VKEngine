#include "Commands.hpp"
#include "VulkanContext.hpp"

#include <iostream>

Commands::Commands(VulkanContext& context, uint32_t maxFramesInFlight)
	: m_context(context), m_maxFramesInFlight(maxFramesInFlight)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_context.getGraphicsQueueFamilyIndex();

	if (vkCreateCommandPool(m_context.getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
	std::cout << "Command Pool created successfully" << std::endl;

	m_commandBuffers.resize(m_maxFramesInFlight);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

	if (vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}
	std::cout << "Command Buffer allocated successfully" << std::endl;
}

Commands::~Commands()
{
	vkDestroyCommandPool(m_context.getDevice(), m_commandPool, nullptr);
}
