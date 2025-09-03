#include <iostream>

#include "Swapchain.hpp"
#include "VulkanContext.hpp" 

Swapchain::Swapchain(VulkanContext& context): m_context(context)
{
	querySurfaceCapabilities();
	pickSurfaceFormat();
	pickPresentMode();
	createSwapchain();
	createImageViews();
}

Swapchain::~Swapchain()
{
	for (const VkImageView& imageView : m_imageViews)
	{
		vkDestroyImageView(m_context.getDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_context.getDevice(), m_swapchain, nullptr);
}

void Swapchain::pickSurfaceFormat()
{
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.getPhysicalDevice(), m_context.getSurface(), &formatCount, nullptr);
	if (formatCount == 0)
	{
		throw std::runtime_error("No surface formats available!");
	}

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.getPhysicalDevice(), m_context.getSurface(), &formatCount, formats.data());

	m_chosenFormat = formats[0]; // Fallback
	for (const VkSurfaceFormatKHR& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			m_chosenFormat = format;
			break;
		}
	}

	m_format = m_chosenFormat.format;
}

void Swapchain::pickPresentMode()
{
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.getPhysicalDevice(), m_context.getSurface(), &presentModeCount, nullptr);
	if (presentModeCount == 0)
	{
		throw std::runtime_error("No present modes available!");
	}

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.getPhysicalDevice(), m_context.getSurface(), &presentModeCount, presentModes.data());

	for (const VkPresentModeKHR& presentMode : presentModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			m_chosenPresentMode = presentMode;
			break;
		}
	}
}

void Swapchain::querySurfaceCapabilities()
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.getPhysicalDevice(), m_context.getSurface(), &m_surfaceCapabilities);
}

void Swapchain::createSwapchain()
{
	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = m_context.getSurface();

	uint32_t imageCount = m_surfaceCapabilities.minImageCount + 1;
	if (m_surfaceCapabilities.maxImageCount > 0 && imageCount > m_surfaceCapabilities.maxImageCount)
	{
		imageCount = m_surfaceCapabilities.maxImageCount;
	}

	m_extent = m_surfaceCapabilities.currentExtent;

	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageFormat = m_chosenFormat.format;
	swapchainCreateInfo.imageColorSpace = m_chosenFormat.colorSpace;
	swapchainCreateInfo.imageExtent = m_extent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform = m_surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = m_chosenPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_context.getDevice(), &swapchainCreateInfo, nullptr, &m_swapchain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swapchain!");
	}
	std::cout << "Swapchain created successfully" << std::endl;

	// Retrieve swapchain images
	vkGetSwapchainImagesKHR(m_context.getDevice(), m_swapchain, &imageCount, nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(m_context.getDevice(), m_swapchain, &imageCount, m_images.data());
}

void Swapchain::createImageViews()
{
	m_imageViews.resize(m_images.size());
	for (size_t i = 0; i < m_images.size(); ++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_images[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_chosenFormat.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_context.getDevice(), &imageViewCreateInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Could not create Image View!");
		}
		std::cout << "Image View " << i << " created successfully" << std::endl;
	}
}