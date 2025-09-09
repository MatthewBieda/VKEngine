#pragma once

#include "volk.h"
#include "Vertex.hpp"

#include "vk_mem_alloc.h"

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
	VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;

	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_indexAllocation = VK_NULL_HANDLE;

	void createVertexBuffer(const std::vector<Vertex>& vertices);
	void createIndexBuffer(const std::vector<uint16_t>& indices);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};