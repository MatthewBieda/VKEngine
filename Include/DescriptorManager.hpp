#pragma once

#include "volk.h"

#include <vector>

class VulkanContext;
class GPUBuffer;
class GPUImage;

class DescriptorManager
{
public:
	DescriptorManager(VulkanContext& context, GPUBuffer& buffer, GPUImage& image);
	~DescriptorManager();

	VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }
	VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
	VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

	void updateUBOdescriptor(size_t currentFrame, VkDeviceSize uniformBufferSize);

private:
	VulkanContext& m_context;
	GPUBuffer& m_buffer;
	GPUImage& m_image;

	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	uint32_t m_maxFramesInFlight{};

	void createDescriptorPool();
	void createDescriptorSetLayout();
	void createDescriptorSet();
};
