#pragma once

#include "volk.h"
#include "VulkanContext.hpp"

#include <string>

class Commands;

class GPUImage
{
public:
	GPUImage(VulkanContext& context, Commands& commands, const std::string& path, VkExtent2D extent);

	// Depth only constructor
	GPUImage(VulkanContext& context, Commands& commands, VkExtent2D extent);

	~GPUImage();

	// Load image from file and upload to GPU (creates image, view and sampler)
	void createTextureImage(const std::string& path);
	void createDepthImage(uint32_t width, uint32_t height);

	VkImage getImage() const { return m_textureImage; }
	VkImageView getImageView() const { return m_textureImageView; }
	VkSampler getSampler() const { return m_textureSampler; }

	VkImageView getDepthImageView() const { return m_depthImageView; }
	VkImage getDepthImage() const { return m_depthImage; }
	VkFormat getDepthFormat() const { return m_depthFormat; }

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkImage m_textureImage = VK_NULL_HANDLE;
	VmaAllocation m_textureImageAllocation = VK_NULL_HANDLE;
	VkImageView m_textureImageView = VK_NULL_HANDLE;
	VkSampler m_textureSampler = VK_NULL_HANDLE;

	VkImage m_depthImage = VK_NULL_HANDLE;
	VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
	VkImageView m_depthImageView = VK_NULL_HANDLE;
	VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

	void transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage textureImage, VkImageAspectFlags aspectMask);
	void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, uint32_t width, uint32_t height);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView& outview);
	void createSampler();

	VkFormat findSupportedDepthFormat();
	bool hasStencil(VkFormat format) const;
};