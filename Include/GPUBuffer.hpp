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
	GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint32_t> indices, uint32_t maxFramesInFlight, VkDeviceSize uniformBufferSize);
	~GPUBuffer();

	VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
	VkBuffer getIndexBuffer() const { return m_indexBuffer; }
	std::vector<VkBuffer> getUniformBuffers() const { return m_uniformBuffers; }
	VkBuffer getUniformBuffer(size_t frameIndex) const { return m_uniformBuffers[frameIndex]; }
	void* getUniformBufferMapped(size_t frameIndex) const { return m_uniformBuffersMapped[frameIndex]; }

	// Utility function for updating uniform data
	void updateUniformBuffer(size_t frameIndex, const void* data, size_t size);

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;

	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_indexAllocation = VK_NULL_HANDLE;

	// Uniform buffer (one per frame in flight)
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VmaAllocation> m_uniformAllocations;
	std::vector<void*> m_uniformBuffersMapped{};

	uint32_t m_maxFramesInFlight{};

	void createVertexBuffer(const std::vector<Vertex>& vertices);
	void createIndexBuffer(const std::vector<uint32_t>& indices);
	void createUniformBuffers(VkDeviceSize bufferSize);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};
