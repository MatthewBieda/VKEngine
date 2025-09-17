#pragma once

#include "volk.h"
#include "VulkanContext.hpp"

#include <string>

class Commands;

class GPUImage
{
public:
	GPUImage(VulkanContext& context, Commands& commands, const std::string& path, VkExtent2D extent);
	GPUImage(VulkanContext& context, Commands& commands, VkExtent2D extent);
	~GPUImage();

	// Load image from file and upload to GPU (creates image, view and sampler)
	void createTextureImage(const std::string& path);
	void createDepthImage(uint32_t width, uint32_t height);
	void createMSAAColorImage(uint32_t width, uint32_t height, VkFormat colorFormat);

	VkImage getImage() const { return m_textureImage; }
	VkImageView getImageView() const { return m_textureImageView; }
	VkSampler getSampler() const { return m_textureSampler; }

	VkImageView getDepthImageView() const { return m_depthImageView; }
	VkImage getDepthImage() const { return m_depthImage; }
	VkFormat getDepthFormat() const { return m_depthFormat; }

	VkImageView getMSAAColorImageView() const { return m_msaaColorImageView; }
	VkSampleCountFlagBits getMSAASamples() const { return m_msaaSamples; }

private:
	VulkanContext& m_context;
	Commands& m_commands;

	// Texture resources (for sampling in shaders, always single sample)
	VkImage m_textureImage = VK_NULL_HANDLE;
	VmaAllocation m_textureImageAllocation = VK_NULL_HANDLE;
	VkImageView m_textureImageView = VK_NULL_HANDLE;
	VkSampler m_textureSampler = VK_NULL_HANDLE;
	uint32_t m_mipLevels = 1;

	// Depth resources (multisampled)
	VkImage m_depthImage = VK_NULL_HANDLE;
	VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
	VkImageView m_depthImageView = VK_NULL_HANDLE;
	VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

	// MSAA resources
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_4_BIT;
	VkImage m_msaaColorImage = VK_NULL_HANDLE;
	VmaAllocation m_msaaColorImageAllocation = VK_NULL_HANDLE;
	VkImageView m_msaaColorImageView = VK_NULL_HANDLE;

	void generateMipmaps(VkCommandBuffer cmd, uint32_t width, uint32_t height);
	void transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage textureImage, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t mipLevelCount);
	void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, uint32_t width, uint32_t height);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView& outview);
	void createSampler();

	VkFormat findSupportedDepthFormat();
	bool hasStencil(VkFormat format) const;
};