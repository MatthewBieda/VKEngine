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
	GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint32_t> indices, VkDeviceSize objectBufferSize, uint32_t maxFramesInFlight);
	~GPUBuffer();

	VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
	VkBuffer getIndexBuffer() const { return m_indexBuffer; }

	void createObjectBuffer(size_t maxObjects);
	VkBuffer getObjectBuffer() const { return m_objectBuffer; }
	size_t getObjectBufferSize() const { return m_objectBufferSize; }
	void* getObjectBufferMapped() const { return m_objectBufferMapped; }

	void createLightingBuffer(VkDeviceSize lightingBufferSize);
	VkBuffer getLightingBuffer() const { return m_lightingBuffer; }
	VkDeviceSize getLightingBufferSize() const { return m_lightingBufferSize; }
	VkDeviceSize getAlignedLightingSize() const { return m_alignedLightingSize; }

	void updateObjectBuffer(const void* data, size_t size);
	void updateLightingBuffer(const void* data, size_t size, uint32_t currentFrame);

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;

	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_indexAllocation = VK_NULL_HANDLE;

	// Storage buffer for per-object data (SSBO)
	VkBuffer m_objectBuffer = VK_NULL_HANDLE;
	VmaAllocation m_objectAllocation = VK_NULL_HANDLE;
	void* m_objectBufferMapped = nullptr;
	VkDeviceSize m_objectBufferSize = 0;

	// Lighting SSBO
	VkBuffer m_lightingBuffer = VK_NULL_HANDLE;
	VmaAllocation m_lightingAllocation = VK_NULL_HANDLE;
	void* m_lightingBufferMapped = nullptr;
	VkDeviceSize m_lightingBufferSize = 0;
	VkDeviceSize m_alignedLightingSize = 0;

	uint32_t m_maxFramesInFlight;

	void createVertexBuffer(const std::vector<Vertex>& vertices);
	void createIndexBuffer(const std::vector<uint32_t>& indices);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};
