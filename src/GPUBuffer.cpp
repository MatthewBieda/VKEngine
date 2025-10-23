#include "Utils.hpp"

#include "GPUBuffer.hpp"
#include "VulkanContext.hpp"
#include "Commands.hpp"

#include <stdexcept>
#include <iostream>

GPUBuffer::GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint32_t> indices, VkDeviceSize objectBufferSize, uint32_t maxFramesInFlight)
	: m_context(context), m_commands(commands), m_objectBufferSize(objectBufferSize), m_maxFramesInFlight(maxFramesInFlight)
{
	createVertexBuffer(vertices);
	createIndexBuffer(indices);
}

GPUBuffer::~GPUBuffer()
{
	vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_objectBuffer, m_objectAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_lightingBuffer, m_lightingAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_visibleIndexBuffer, m_visibleIndexAllocation);
}

void GPUBuffer::updateObjectBuffer(const void* data, size_t size, uint32_t currentFrame)
{
	if (size > m_alignedObjectSize)
	{
		throw std::runtime_error("Object buffer overflow!");
	}

	VkDeviceSize offset = currentFrame * m_alignedObjectSize;
	memcpy((char*)m_objectBufferMapped + offset, data, size);
}

void GPUBuffer::updateLightingBuffer(const void* data, size_t size, uint32_t currentFrame)
{
	if (size > m_alignedLightingSize)
	{
		throw std::runtime_error("Light buffer overflow!");
	}

	VkDeviceSize offset = currentFrame * m_alignedLightingSize;
	memcpy((char*)m_lightingBufferMapped + offset, data, size);
}

void GPUBuffer::updateVisibleIndexBuffer(const void* data, size_t size, uint32_t currentFrame)
{
	// where size is sizeof(uint32_t) * visibleIndices.size();
	if (size > m_alignedVisbleIndexBufferSize)
	{
		throw std::runtime_error("Visible index buffer overflow");
	}

	VkDeviceSize offset = currentFrame * m_alignedVisbleIndexBufferSize;
	memcpy((char*)m_visibleIndexBufferMapped + offset, data, size);
}

void GPUBuffer::createVertexBuffer(const std::vector<Vertex>& vertices)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// staging buffer
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	VkBufferCreateInfo  bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create staging buffer");
	}

	// map + copy
	void* data;
	vmaMapMemory(m_context.getAllocator(), stagingAllocation, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vmaUnmapMemory(m_context.getAllocator(), stagingAllocation);

	// GPU local buffer
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_vertexBuffer, &m_vertexAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer");
	};
	nameObject(m_context.getDevice(), m_vertexBuffer, "VertexBuffer_Main");
	std::cout << "Vertex Buffer created successfully" << std::endl;

	// copy staging -> GPU
	copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

	// cleanup
	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);
}

void GPUBuffer::createIndexBuffer(const std::vector<uint32_t>& indices)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// staging buffer
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	VkBufferCreateInfo  bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Staging Buffer");
	}

	// map + copy
	void* data;
	vmaMapMemory(m_context.getAllocator(), stagingAllocation, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vmaUnmapMemory(m_context.getAllocator(), stagingAllocation);

	// GPU local buffer
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_indexBuffer, &m_indexAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Index Buffer");
	};
	nameObject(m_context.getDevice(), m_indexBuffer, "IndexBuffer_Main");
	std::cout << "Index Buffer created successfully" << std::endl;

	// copy staging -> GPU
	copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

	// cleanup
	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);
}

void GPUBuffer::createLightingBuffer(VkDeviceSize lightingBufferSize)
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &props);

	VkDeviceSize alignment = props.limits.minStorageBufferOffsetAlignment;
	m_lightingBufferSize = lightingBufferSize;

	// Round size up to the next multiple of alignment
	VkDeviceSize alignedLightingSize = (m_lightingBufferSize + alignment - 1) & ~(alignment - 1);
	m_alignedLightingSize = alignedLightingSize;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_alignedLightingSize * m_maxFramesInFlight;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_lightingBuffer, &m_lightingAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create lighting SSBO");
	}

	// Get mapped pointer
	VmaAllocationInfo allocInfoDetails{};
	vmaGetAllocationInfo(m_context.getAllocator(), m_lightingAllocation, &allocInfoDetails);
	m_lightingBufferMapped = allocInfoDetails.pMappedData;

	nameObject(m_context.getDevice(), m_lightingBuffer, "LightingBuffer_SSBO");
	std::cout << "Lighting dynamic SSBO created successfully" << std::endl;
}

void GPUBuffer::createObjectBuffer(size_t maxObjects)
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &props);

	VkDeviceSize alignment = props.limits.minStorageBufferOffsetAlignment;
	VkDeviceSize singleObjectBufferSize = m_objectBufferSize * maxObjects;

	m_alignedObjectSize = (singleObjectBufferSize + alignment - 1) & ~(alignment - 1);

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_alignedObjectSize * m_maxFramesInFlight;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_objectBuffer, &m_objectAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create object SSBO");
	}

	VmaAllocationInfo allocInfoDetails{};
	vmaGetAllocationInfo(m_context.getAllocator(), m_objectAllocation, &allocInfoDetails);
	m_objectBufferMapped = allocInfoDetails.pMappedData;

	nameObject(m_context.getDevice(), m_objectBuffer, "ObjectBuffer_SSBO");
	std::cout << "Object dynamic SSBO created successfully (" << maxObjects << " objects)" << std::endl;
}

void GPUBuffer::createVisibleIndexBuffer(size_t maxObjects)
{
	// Buffer must be large enough tn hold all object indices
	m_visibleIndexBufferSize = sizeof(uint32_t) * maxObjects; // Size of one index * amount of object

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &props);
	VkDeviceSize alignment = props.limits.minStorageBufferOffsetAlignment;

	m_alignedVisbleIndexBufferSize = (m_visibleIndexBufferSize + alignment - 1) & ~(alignment - 1);

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_alignedVisbleIndexBufferSize * m_maxFramesInFlight;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_visibleIndexBuffer, &m_visibleIndexAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create visible index SSBO");
	}

	VmaAllocationInfo allocInfoDetails{};
	vmaGetAllocationInfo(m_context.getAllocator(), m_visibleIndexAllocation, &allocInfoDetails);
	m_visibleIndexBufferMapped = allocInfoDetails.pMappedData;

	nameObject(m_context.getDevice(), m_visibleIndexBuffer, "VisibleIndexBuffer_SSBO");
	std::cout << "Visible Index dynamic SSBO created successfully" << std::endl;
}


void GPUBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = m_commands.beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	m_commands.endSingleTimeCommands(commandBuffer);
}
