#pragma once

#include "volk.h"
#include "VulkanContext.hpp"

#include <string>

class Commands;

class GPUImage
{
public:
	GPUImage(VulkanContext& context, Commands& commands, const std::string& path);
	~GPUImage();

	// Load image from file and upload to GPU (creates image, view and sampler
	void createTextureImage(const std::string& path);

	VkImage getImage() const { return m_textureImage; }
	VkImageView getImageView() const { return m_textureImageView; }
	VkSampler getSampler() const { return m_textureSampler; }

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkImage m_textureImage = VK_NULL_HANDLE;
	VmaAllocation m_textureImageAllocation = VK_NULL_HANDLE;
	VkImageView m_textureImageView = VK_NULL_HANDLE;
	VkSampler m_textureSampler = VK_NULL_HANDLE;

	void transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, uint32_t width, uint32_t height);

	void createImageView(VkFormat format);
	void createSampler();
};