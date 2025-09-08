#pragma once

#include "volk.h"
#include <vector>

class VulkanContext;

class Swapchain
{
public:
	Swapchain(VulkanContext& context);

	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;
	Swapchain(Swapchain&&) = delete;
	Swapchain& operator=(Swapchain&&) = delete;

	~Swapchain();

	VkSwapchainKHR getSwapchain() const { return m_swapchain; }
	VkFormat getFormat() const { return m_format; }
	VkExtent2D getExtent() const { return m_extent; }
	const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }
	const std::vector<VkImage>& getImages() const { return m_images; }
	uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }

	VkImage getSwapchainImage(uint32_t frameIndex) const { return m_images[frameIndex]; }
	VkImageView getSwapchainImageView(uint32_t frameIndex) const { return m_imageViews[frameIndex]; }

private:
	void querySurfaceCapabilities();
	void pickSurfaceFormat();
	void pickPresentMode();
	void createSwapchain();
	void createImageViews();

	VulkanContext& m_context;

	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	VkFormat m_format;
	VkExtent2D m_extent;

	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_imageViews;

	VkSurfaceFormatKHR m_chosenFormat{};
	VkPresentModeKHR m_chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkSurfaceCapabilitiesKHR m_surfaceCapabilities{};
};