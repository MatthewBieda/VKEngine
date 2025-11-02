#include "Utils.hpp"

#include "VulkanContext.hpp"
#include "GPUBuffer.hpp"
#include "GPUImage.hpp"
#include "DescriptorManager.hpp"

#include "imgui_impl_vulkan.h"

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

void DescriptorManager::updateTextureArray(const std::vector<VkImageView>& textureViews, VkSampler sampler)
{
	std::vector<VkDescriptorImageInfo> imageInfos(textureViews.size());
	for (size_t i = 0; i < textureViews.size(); ++i)
	{
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = textureViews[i];
		imageInfos[i].sampler = sampler;
	}

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = m_descriptorSet;
	write.dstBinding = 1;
	write.dstArrayElement = 0;
	write.descriptorCount = static_cast<uint32_t>(imageInfos.size());
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_context.getDevice(), 1, &write, 0, nullptr);
}

void DescriptorManager::createDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 6> bindings{};

	// Storage buffer for per-object data
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Texture binding
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1000;
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

	// Visible Index data
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Shadow map
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1; // One shadow map for now
	bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Enable descriptor indexing flags
	VkDescriptorBindingFlags bindingFlags[] = {
		0, // binding 0: Object data
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // binding 1: Texture array
		0, // binding 2: Lighting data
		0, // binding 3: Cubemap data
		0, // binding 4: Visible index data
		0  // binding 5: Shadow Map
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = 6;
	bindingFlagsInfo.pBindingFlags = bindingFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	layoutInfo.pNext = &bindingFlagsInfo;

	if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout");
	}
	std::cout << "Descriptor Set Layout created successfully" << std::endl;
	nameObject(m_context.getDevice(), m_descriptorSetLayout, "DesctiptorSetLayout_Global");
}

void DescriptorManager::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 3 }; // Per-instance data + lighting + Visible indexes
	poolSizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1002 }; // object texture + skybox + shadowmap

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

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

	// Persistent per-object SSBO
	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = m_buffer.getObjectBuffer();
	ssboInfo.offset = 0;
	ssboInfo.range = m_buffer.getObjectBufferSize();

	// Lighting (dynamic) buffer info
	VkDescriptorBufferInfo lightingInfo{};
	lightingInfo.buffer = m_buffer.getLightingBuffer();
	lightingInfo.offset = 0;
	lightingInfo.range = m_buffer.getLightingBufferSize();

	// Cubemap info
	VkDescriptorImageInfo cubemapInfo{};
	cubemapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cubemapInfo.imageView = m_image.getSkyboxImageView();
	cubemapInfo.sampler = m_image.getSampler();

	// Visible Index Buffer Info
	VkDescriptorBufferInfo visibleIndexInfo{};
	visibleIndexInfo.buffer = m_buffer.getVisibleIndexBuffer();
	visibleIndexInfo.offset = 0;
	visibleIndexInfo.range = m_buffer.getVisibleIndexBufferSize();

	// Shadow map info
	VkDescriptorImageInfo shadowMapInfo{};
	shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	shadowMapInfo.imageView = m_image.getShadowMaps()[0].view;
	shadowMapInfo.sampler = m_image.getShadowSampler();

	std::array<VkWriteDescriptorSet, 5> persistentWrites{};

	// Per-instance SSBO
	persistentWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[0].dstSet = m_descriptorSet;
	persistentWrites[0].dstBinding = 0;
	persistentWrites[0].descriptorCount = 1;
	persistentWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	persistentWrites[0].pBufferInfo = &ssboInfo;

	// Lighting SSBO
	persistentWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[1].dstSet = m_descriptorSet;
	persistentWrites[1].dstBinding = 2;
	persistentWrites[1].descriptorCount = 1;
	persistentWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	persistentWrites[1].pBufferInfo = &lightingInfo;

	// Cubemap binding
	persistentWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[2].dstSet = m_descriptorSet;
	persistentWrites[2].dstBinding = 3;
	persistentWrites[2].descriptorCount = 1;
	persistentWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	persistentWrites[2].pImageInfo = &cubemapInfo;

	// Visible index binding
	persistentWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[3].dstSet = m_descriptorSet;
	persistentWrites[3].dstBinding = 4;
	persistentWrites[3].descriptorCount = 1;
	persistentWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	persistentWrites[3].pBufferInfo = &visibleIndexInfo;

	// Shadow map binding
	persistentWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[4].dstSet = m_descriptorSet;
	persistentWrites[4].dstBinding = 5;
	persistentWrites[4].descriptorCount = 1;
	persistentWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	persistentWrites[4].pImageInfo = &shadowMapInfo;

	vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(persistentWrites.size()), persistentWrites.data(), 0, nullptr);
}
