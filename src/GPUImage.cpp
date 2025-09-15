#include "Commands.hpp"
#include "GPUImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>

GPUImage::GPUImage(VulkanContext& context, Commands& commands, const std::string& path, VkExtent2D extent)
	: m_context(context), m_commands(commands) 
{
	createTextureImage(path);
	createDepthImage(extent.width, extent.height);
}

GPUImage::GPUImage(VulkanContext& context, Commands& commands, VkExtent2D extent): m_context(context), m_commands(commands)
{
	createDepthImage(extent.width, extent.height);
}

GPUImage::~GPUImage()
{
	vkDestroySampler(m_context.getDevice(), m_textureSampler, nullptr);
	vkDestroyImageView(m_context.getDevice(), m_textureImageView, nullptr);
	vmaDestroyImage(m_context.getAllocator(), m_textureImage, m_textureImageAllocation);

	vkDestroyImageView(m_context.getDevice(), m_depthImageView, nullptr);
	vmaDestroyImage(m_context.getAllocator(), m_depthImage, m_depthImageAllocation);
}

void GPUImage::createTextureImage(const std::string& path)
{
	int texWidth, texHeight, texChannels;
	uint8_t* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		throw std::runtime_error("Failed to load texture image!");
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	// Create staging buffer (host visible)
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = imageSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create staging buffer for texture");
	}

	// Map and copy pixel data
	void* mapped = nullptr;
	vmaMapMemory(m_context.getAllocator(), stagingAllocation, &mapped);
	memcpy(mapped, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(m_context.getAllocator(), stagingAllocation);

	// Free CPU pixels now they are in staging buffer
	stbi_image_free(pixels);

	// Create GPU Image (Device local)
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(texWidth);
	imageInfo.extent.height = static_cast<uint32_t>(texHeight);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	VmaAllocationCreateInfo imgAllocInfo{};
	imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateImage(m_context.getAllocator(), &imageInfo, &imgAllocInfo, &m_textureImage, &m_textureImageAllocation, nullptr) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create GPU image");
	}

	// Record all commands in one command buffer
	VkCommandBufferAllocateInfo allocInfoCmd{};
	allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfoCmd.commandPool = m_commands.getCommandPool();
	allocInfoCmd.commandBufferCount = 1;

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(m_context.getDevice(), &allocInfoCmd, &cmd);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Transition, copy, transition
	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT);
	copyBufferToImage(cmd, stagingBuffer, texWidth, texHeight);
	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_context.getGraphicsQueue());

	vkFreeCommandBuffers(m_context.getDevice(), m_commands.getCommandPool(), 1, &cmd);

	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);

	// Create view and sampler
	createImageView(m_textureImage, imageInfo.format, VK_IMAGE_ASPECT_COLOR_BIT, m_textureImageView);
	createSampler();
}

void GPUImage::createDepthImage(uint32_t width, uint32_t height)
{
	m_depthFormat = findSupportedDepthFormat();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_depthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create depth image");
	}

	// Record all commands in one command buffer
	VkCommandBufferAllocateInfo allocInfoCmd{};
	allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfoCmd.commandPool = m_commands.getCommandPool();
	allocInfoCmd.commandBufferCount = 1;

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(m_context.getDevice(), &allocInfoCmd, &cmd);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Transition depth layout
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (hasStencil(m_depthFormat))
	{
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, m_depthImage, aspect);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_context.getGraphicsQueue());

	vkFreeCommandBuffers(m_context.getDevice(), m_commands.getCommandPool(), 1, &cmd);

	createImageView(m_depthImage, m_depthFormat, aspect, m_depthImageView);

}

void GPUImage::transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkImageAspectFlags aspectMask)
{
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags2 srcStage, dstStage;
	VkAccessFlags2 srcAccess, dstAccess;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		srcAccess = 0;
		dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		srcAccess = 0;
		dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	else
	{
		throw std::invalid_argument("Unsupported layout transition");
	}

	barrier.srcStageMask = srcStage;
	barrier.dstStageMask = dstStage;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void GPUImage::copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(cmd, buffer, m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void GPUImage::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView& outview)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &outview) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image view");
	}
}

void GPUImage::createSampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}
}

VkFormat GPUImage::findSupportedDepthFormat()
{
	std::vector<VkFormat> candidates = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT
	};
	
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), format, &props);

		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a supported depth format!");
}

bool GPUImage::hasStencil(VkFormat format) const
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
