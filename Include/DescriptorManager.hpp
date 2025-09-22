#pragma once

#include "volk.h"

#include <vector>

class VulkanContext;
class GPUBuffer;
class GPUImage;

class DescriptorManager
{
public:
	DescriptorManager(VulkanContext& context, GPUBuffer& buffer, GPUImage& image, uint32_t maxFramesInFlight, VkDeviceSize uniformBufferSize);
	~DescriptorManager();

	VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
	VkDescriptorSet getDescriptorSet(size_t frameIndex) { return m_descriptorSets[frameIndex]; }
	VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
	VulkanContext& m_context;
	GPUBuffer& m_buffer;
	GPUImage& m_image;

	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> m_descriptorSets{};

	uint32_t m_maxFramesInFlight{};

	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets(VkDeviceSize uniformBufferSize);
};
