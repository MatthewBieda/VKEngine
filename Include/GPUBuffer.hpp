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
	VkDeviceSize getAlignedObjectSize() const { return m_alignedObjectSize; }
	void* getObjectBufferMapped() const { return m_objectBufferMapped; }

	void createLightingBuffer(VkDeviceSize lightingBufferSize);
	VkBuffer getLightingBuffer() const { return m_lightingBuffer; }
	VkDeviceSize getLightingBufferSize() const { return m_lightingBufferSize; }
	VkDeviceSize getAlignedLightingSize() const { return m_alignedLightingSize; }

	void createVisibleIndexBuffer(size_t maxObjects);
	VkBuffer getVisibleIndexBuffer() const { return m_visibleIndexBuffer; }
	VkDeviceSize getVisibleIndexBufferSize() const { return m_visibleIndexBufferSize; }
	VkDeviceSize getAlignedVisibleIndexBufferSize() const { return m_alignedVisbleIndexBufferSize; }

	void updateObjectBuffer(const void* data, size_t size, uint32_t currentFrame);
	void updateLightingBuffer(const void* data, size_t size, uint32_t currentFrame);
	void updateVisibleIndexBuffer(const void* data, size_t size, uint32_t currentFrame);

private:
	VulkanContext& m_context;
	Commands& m_commands;

	VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_vertexAllocation = VK_NULL_HANDLE;

	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_indexAllocation = VK_NULL_HANDLE;

	// Per-instance data SSBO
	VkBuffer m_objectBuffer = VK_NULL_HANDLE;
	VmaAllocation m_objectAllocation = VK_NULL_HANDLE;
	void* m_objectBufferMapped = nullptr;
	VkDeviceSize m_objectBufferSize = 0;
	VkDeviceSize m_alignedObjectSize = 0;

	// Lighting SSBO
	VkBuffer m_lightingBuffer = VK_NULL_HANDLE;
	VmaAllocation m_lightingAllocation = VK_NULL_HANDLE;
	void* m_lightingBufferMapped = nullptr;
	VkDeviceSize m_lightingBufferSize = 0;
	VkDeviceSize m_alignedLightingSize = 0;

	// Visible Instance Index SSBO
	VkBuffer m_visibleIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation m_visibleIndexAllocation = VK_NULL_HANDLE;
	void* m_visibleIndexBufferMapped = nullptr;
	VkDeviceSize m_visibleIndexBufferSize = 0;
	VkDeviceSize m_alignedVisbleIndexBufferSize = 0;

	uint32_t m_maxFramesInFlight;

	void createVertexBuffer(const std::vector<Vertex>& vertices);
	void createIndexBuffer(const std::vector<uint32_t>& indices);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};
