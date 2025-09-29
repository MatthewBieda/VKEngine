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
	std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	std::array<VkDescriptorBindingFlags, 3> flags{};
	flags[0] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
	flags[1] = 0;
	flags[2] = 0; // smapler
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.bindingCount = static_cast<uint32_t>(flags.size());
	bindingFlags.pBindingFlags = flags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	layoutInfo.pNext = &bindingFlags;

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
	poolSizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }; // one UBO binding
	poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }; // one SSBO binding
	poolSizes[2] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }; // one texture binding

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 2;

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

	// Initialize SSBO + texture descriptor (persistent)

	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = m_buffer.getObjectBuffer();
	ssboInfo.offset = 0;
	ssboInfo.range = m_buffer.getObjectBufferSize();

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = m_image.getImageView();
	imageInfo.sampler = m_image.getSampler();

	std::array<VkWriteDescriptorSet, 2> persistentWrites{};

	// SSBO binding
	persistentWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[0].dstSet = m_descriptorSet;
	persistentWrites[0].dstBinding = 1;
	persistentWrites[0].descriptorCount = 1;
	persistentWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	persistentWrites[0].pBufferInfo = &ssboInfo;

	// Texture binding
	persistentWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	persistentWrites[1].dstSet = m_descriptorSet;
	persistentWrites[1].dstBinding = 2;
	persistentWrites[1].descriptorCount = 1;
	persistentWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	persistentWrites[1].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(persistentWrites.size()), persistentWrites.data(), 0, nullptr);
	
}


void DescriptorManager::updateUBOdescriptor(size_t currentFrame, VkDeviceSize uniformBufferSize)
{
	VkDescriptorBufferInfo uboInfo{};
	uboInfo.buffer = m_buffer.getUniformBuffer(currentFrame);
	uboInfo.offset = 0;
	uboInfo.range = uniformBufferSize;

	VkWriteDescriptorSet uboWrite{};
	uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uboWrite.dstSet = m_descriptorSet;
	uboWrite.dstBinding = 0;
	uboWrite.dstArrayElement = 0;
	uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboWrite.descriptorCount = 1;
	uboWrite.pBufferInfo = &uboInfo;

	vkUpdateDescriptorSets(m_context.getDevice(), 1, &uboWrite, 0, nullptr);
}