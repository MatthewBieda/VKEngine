#include "Sync.hpp"
#include "VulkanContext.hpp"
#include "Swapchain.hpp"

#include <iostream>
#include <string>

Sync::Sync(VulkanContext& context, Swapchain& swapchain, uint32_t maxFramesInFlight)
	: m_context(context), m_swapchain(swapchain), m_maxFramesInFlight(maxFramesInFlight)
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_imageAvailableSemaphores.resize(maxFramesInFlight);
	m_renderFinishedSemaphores.resize(m_swapchain.getImageCount());
	m_inFlightFences.resize(maxFramesInFlight);

	for (size_t i = 0; i < m_maxFramesInFlight; ++i)
	{
		if (vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create synchronization primitives for frame " + std::to_string(i));
		}
		std::cout << "Syncronization primitives for frame: " << i << " created successfully" << std::endl;
	}

	for (size_t i = 0; i < m_swapchain.getImageCount(); ++i)
	{
		if (vkCreateSemaphore(m_context.getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render finished semaphore for image " + std::to_string(i));
		}
		std::cout << "Render finished semaphore for frame: " << i << " created successfully" << std::endl;
	}
}

Sync::~Sync()
{
	for (const VkFence& fence : m_inFlightFences)
	{
		vkDestroyFence(m_context.getDevice(), fence, nullptr);
	}
	for (const VkSemaphore& semaphore : m_renderFinishedSemaphores)
	{
		vkDestroySemaphore(m_context.getDevice(), semaphore, nullptr);
	}
	for (const VkSemaphore& semaphore : m_imageAvailableSemaphores)
	{
		vkDestroySemaphore(m_context.getDevice(), semaphore, nullptr);
	}
}
