#include <iostream>
#include <algorithm>

#include "Utils.hpp"

#include "Swapchain.hpp"
#include "VulkanContext.hpp" 

Swapchain::Swapchain(GLFWwindow* window, VulkanContext& context): m_window(window), m_context(context)
{
	querySurfaceCapabilities();
	chooseSwapExtent();
	pickSurfaceFormat();
	pickPresentMode();
	createSwapchain();
	createImageViews();
}

Swapchain::~Swapchain()
{
	cleanupSwapchain();
}

void Swapchain::cleanupSwapchain()
{
	for (const VkImageView& imageView : m_imageViews)
	{
		vkDestroyImageView(m_context.getDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_context.getDevice(), m_swapchain, nullptr);
}

void Swapchain::recreateSwapchain()
{
	// Handle window minimization
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_context.getDevice());

	cleanupSwapchain();

	std::cout << "=== RECREATING SWAPCHAIN ===" << std::endl;
	querySurfaceCapabilities();  // Re-query surface capabilities (CRITICAL!)
	chooseSwapExtent();          // Calculate new extent with new capabilities
	pickSurfaceFormat();         // Re-pick format (might have changed)
	pickPresentMode();           // Re-pick present mode (might have changed)
	createSwapchain();           // Create with all updated values
	createImageViews();          // Create views for new images
	std::cout << "=== SWAPCHAIN RECREATION COMPLETE ===" << std::endl;

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

void Swapchain::chooseSwapExtent()
{
	if (m_surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		std::cout << "Using surface current extent: " << m_surfaceCapabilities.currentExtent.width << "x" <<
			m_surfaceCapabilities.currentExtent.height << std::endl;
		m_extent = m_surfaceCapabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		std::cout << "Surface extent is undefined, using framebuffer size: "
				  << width << "x" << height << std::endl;

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width,
										 m_surfaceCapabilities.minImageExtent.width,
										 m_surfaceCapabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height,
										 m_surfaceCapabilities.minImageExtent.height,
										 m_surfaceCapabilities.maxImageExtent.height);

		std::cout << "Clamped extent: " << actualExtent.width << "X" << actualExtent.height << std::endl;
		m_extent = actualExtent;
	}
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

	std::cout << "Creating swapchain with extent: " << m_extent.width << "x" << m_extent.height << std::endl;

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
	nameObject(m_context.getDevice(), m_swapchain, "Swapchain");

	// Retrieve swapchain images
	vkGetSwapchainImagesKHR(m_context.getDevice(), m_swapchain, &imageCount, nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(m_context.getDevice(), m_swapchain, &imageCount, m_images.data());

	nameObjects(m_context.getDevice(), m_images, "Image_Swapchain_");
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
		std::cout << "Swapchain Image View " << i << " created successfully" << std::endl;
	}
	nameObjects(m_context.getDevice(), m_imageViews, "ImageView_Swapchain_");
}
