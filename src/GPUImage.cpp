#include "Commands.hpp"
#include "GPUImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
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
	vkDestroyImageView(m_context.getDevice(), m_msaaColorImageView, nullptr);
	vmaDestroyImage(m_context.getAllocator(), m_msaaColorImage, m_msaaColorImageAllocation);

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

	// Calculate mip levels
	m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

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
	imageInfo.mipLevels = m_mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	VmaAllocationCreateInfo imgAllocInfo{};
	imgAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateImage(m_context.getAllocator(), &imageInfo, &imgAllocInfo, &m_textureImage, &m_textureImageAllocation, nullptr) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create GPU image");
	}

	VkCommandBuffer cmd = m_commands.beginSingleTimeCommands();

	// Transition base mip level for transfer
	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

	// Copy buffer to base mip level (mip 0)
	copyBufferToImage(cmd, stagingBuffer, texWidth, texHeight);

	// Generate mipmaps
	generateMipmaps(cmd, texWidth, texHeight);

	m_commands.endSingleTimeCommands(cmd);

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
	imageInfo.samples = m_msaaSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create depth image");
	}

	VkCommandBuffer cmd = m_commands.beginSingleTimeCommands();

	// Transition depth layout
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (hasStencil(m_depthFormat))
	{
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, m_depthImage, aspect, 0, 1);

	m_commands.endSingleTimeCommands(cmd);

	createImageView(m_depthImage, m_depthFormat, aspect, m_depthImageView);
}

void GPUImage::createMSAAColorImage(uint32_t width, uint32_t height, VkFormat colorFormat)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	imageInfo.samples = m_msaaSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo, &m_msaaColorImage, &m_msaaColorImageAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create MSAA color image");
	}

	VkCommandBuffer cmd = m_commands.beginSingleTimeCommands();

	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, m_msaaColorImage, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

	m_commands.endSingleTimeCommands(cmd);

	createImageView(m_msaaColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, m_msaaColorImageView);
}

void GPUImage::generateMipmaps(VkCommandBuffer cmd, uint32_t width, uint32_t height)
{
	// Check if linear blitting is supported for our format
	VkFormatProperties formatProps;
	vkGetPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), VK_FORMAT_R8G8B8A8_SRGB, &formatProps);

	if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("Linear blitting not supported for texture format!");
	}

	int32_t mipWidth = width;
	int32_t mipHeight = height;

	for (uint32_t i = 1; i < m_mipLevels; ++i)
	{
		// Transition previous mip level to TRANSFER_SRC_OPTIMAL for reading
		transitionImageLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

		// Transition next mip level to TRANSFER_DST_OPTIMAL for blitting
		transitionImageLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);

		// Set up blit operation
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0,0,0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;

		// Calculate next mip level dimensions
		int32_t nextMipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
		int32_t nextMipHeight = mipHeight > 1 ? mipHeight / 2 : 1;

		blit.dstOffsets[0] = { 0,0,0 };
		blit.dstOffsets[1] = { nextMipWidth, nextMipHeight, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		// Perform the blit (current mip level i is still in TRANSFER_DST_OPTIMAL)
		vkCmdBlitImage(cmd, m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1, &blit, VK_FILTER_LINEAR);

		// Transition the previous mip level to SHADER_READ_ONLY_OPTIMAL
		transitionImageLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1);

		mipWidth = nextMipWidth;
		mipHeight = nextMipHeight;
	}

	// Transition the last mip level to SHADER_READ_ONLY_OPTIMAL
	transitionImageLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		m_textureImage, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels - 1, 1);
}

void GPUImage::transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t mipLevelCount)
{
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = mipLevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags2 srcStage, dstStage;
	VkAccessFlags2 srcAccess, dstAccess;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_NONE;
		dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		srcAccess = VK_ACCESS_2_NONE;
		dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		dstAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
		dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_NONE;
		dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		srcAccess = VK_ACCESS_2_NONE;
		dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		srcStage = VK_PIPELINE_STAGE_2_NONE;
		dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		srcAccess = VK_ACCESS_2_NONE;
		dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
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
	region.imageSubresource.mipLevel = 0; // Only copying to base mip level
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

	// For texture images, include all mip levels in the view
	if (image == m_textureImage)
	{
		viewInfo.subresourceRange.levelCount = m_mipLevels;
	}
	else 
	{
		viewInfo.subresourceRange.levelCount = 1; // Depth and MSAA won't use mipmaps

	}

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

	// Enable mipmaps
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

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
