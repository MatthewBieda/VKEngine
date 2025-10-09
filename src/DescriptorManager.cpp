#include "Utils.hpp"

#include "VulkanContext.hpp"
#include "GPUBuffer.hpp"
#include "GPUImage.hpp"
#include "DescriptorManager.hpp"

#include <stdexcept>
#include <iostream>
#include <array>

DescriptorManager::DescriptorManager(VulkanContext& context, GPUBuffer& buffer, GPUImage& image)
	: m_context(context), m_buffer(buffer), m_image(image)
{
	createDescriptorSetLayout();
	createDescriptorPool();
	createDescriptorSet();
}

DescriptorManager::~DescriptorManager()
{
	vkDestroyDescriptorPool(m_context.getDevice(), m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descriptorSetLayout, nullptr);
}

void DescriptorManager::createDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

	// Storage buffer for per-object data
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Texture binding
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Lighting data
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Cubemap binding
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout");
	}
	std::cout << "Descriptor Set Layout created successfully" << std::endl;
	nameObject(m_context.getDevice(), m_descriptorSetLayout, "DesctiptorSetLayout_Global");
}

void DescriptorManager::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }; // object data (static)
	poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 }; // Lighting (dynamic)
	poolSizes[2] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 }; // object texture + skybox

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 2; // ImGui needs another set

	if (vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool");
	}
	std::cout << "Descriptor Pool created successfully" << std::endl;
	nameObject(m_context.getDevice(), m_descriptorPool, "DescriptorPool_Global");
}

void DescriptorManager::createDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;

	if (vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, &m_descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor set!");
	}
	std::cout << "Descriptor Set Allocated successfully" << std::endl;
	nameObject(m_context.getDevice(), m_descriptorSet, "DescriptorSet");

	// Persistent SSBO + texture bindings
	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = m_buffer.getObjectBuffer();
	ssboInfo.offset = 0;
	ssboInfo.range = m_buffer.getObjectBufferSize();

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = m_image.getImageView();
	imageInfo.sampler = m_image.getSampler();

	VkDescriptorBufferInfo lightingInfo{};
	lightingInfo.buffer = m_buffer.getLightingBuffer();
	lightingInfo.offset = 0;
	lightingInfo.range = m_buffer.getLightingBufferSize();

	VkDescriptorImageInfo cubemapInfo{};
	cubemapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cubemapInfo.imageView = m_image.getSkyboxImageView();
	cubemapInfo.sampler = m_image.getSampler();

	std::array<VkWriteDescriptorSet, 4> persistentWrites{};

	// SSBO binding
	persistentWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[0].dstSet = m_descriptorSet;
	persistentWrites[0].dstBinding = 0;
	persistentWrites[0].descriptorCount = 1;
	persistentWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	persistentWrites[0].pBufferInfo = &ssboInfo;

	// Texture binding
	persistentWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[1].dstSet = m_descriptorSet;
	persistentWrites[1].dstBinding = 1;
	persistentWrites[1].descriptorCount = 1;
	persistentWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	persistentWrites[1].pImageInfo = &imageInfo;

	// Lighting SSBO
	persistentWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[2].dstSet = m_descriptorSet;
	persistentWrites[2].dstBinding = 2;
	persistentWrites[2].descriptorCount = 1;
	persistentWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	persistentWrites[2].pBufferInfo = &lightingInfo;

	// Cubemap binding
	persistentWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[3].dstSet = m_descriptorSet;
	persistentWrites[3].dstBinding = 3;
	persistentWrites[3].descriptorCount = 1;
	persistentWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	persistentWrites[3].pImageInfo = &cubemapInfo;

	vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(persistentWrites.size()), persistentWrites.data(), 0, nullptr);
}
