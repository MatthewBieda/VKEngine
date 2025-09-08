#pragma once

#include "volk.h"
#include "Vertex.hpp"

#include <vector>

class VulkanContext;
class Commands;

class GPUBuffer
{
public:
	GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint16_t> indices);
	~GPUBuffer();

	VkBuffer getVertexBuffer() { return m_vertexBuffer; }
	VkBuffer getIndexBuffer() { return m_indexBuffer; }

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
	VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
	VkPhysicalDeviceMemoryProperties m_memProperties{};

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void createVertexBuffer(const std::vector<Vertex>& vertices);
	void createIndexBuffer(const std::vector<uint16_t> indices);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};