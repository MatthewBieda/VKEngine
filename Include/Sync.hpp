#pragma once

#include <volk.h>
#include <vector>

class VulkanContext;
class Swapchain;

class Sync
{
public:
	Sync(VulkanContext& context, Swapchain& swapchain, uint32_t maxFramesInFlight);
	~Sync();

	VkSemaphore getImageAvailableSemaphore(uint32_t frameIndex) const
	{
		return m_imageAvailableSemaphores[frameIndex];
	}

	VkSemaphore getRenderFinishedSemaphore(uint32_t frameIndex) const
	{
		return m_renderFinishedSemaphores[frameIndex];
	}

	VkFence getInFlightFence(uint32_t frameIndex) const
	{
		return m_inFlightFences[frameIndex];
	}

	const VkFence* getInFlightFencePtr(uint32_t frameIndex) const
	{
		return &m_inFlightFences[frameIndex];
	}

private:
	VulkanContext& m_context;
	Swapchain& m_swapchain;

	uint32_t m_maxFramesInFlight;

	std::vector<VkSemaphore> m_imageAvailableSemaphores{};
	std::vector<VkSemaphore> m_renderFinishedSemaphores{};
	std::vector<VkFence> m_inFlightFences{};
};
