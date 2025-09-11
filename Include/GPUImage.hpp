#pragma once

#include "volk.h"
#include "VulkanContext.hpp"

class GPUImage
{
public:
	GPUImage(VulkanContext& context);
	~GPUImage();

private:
	VulkanContext& m_context;

	VkImage m_textureImage = VK_NULL_HANDLE;
	VmaAllocation m_textureImageAllocation = VK_NULL_HANDLE;
	VkImageView m_textureImageView = VK_NULL_HANDLE;
	VkSampler m_textureSampler = VK_NULL_HANDLE;

	void createImage();
};