#include "Utils.hpp"

#include "GPUBuffer.hpp"
#include "VulkanContext.hpp"
#include "Commands.hpp"

#include <stdexcept>
#include <iostream>

GPUBuffer::GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint32_t> indices, VkDeviceSize objectBufferSize)
	: m_context(context), m_commands(commands), m_objectBufferSize(objectBufferSize)
{
	createVertexBuffer(vertices);
	createIndexBuffer(indices);
}

GPUBuffer::~GPUBuffer()
{
	vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_objectBuffer, m_objectAllocation);
}

void GPUBuffer::updateObjectBuffer(const void* data, size_t size)
{
	if (size > m_objectBufferSize)
	{
		throw std::runtime_error("Object buffer overflow!");
	}
	memcpy(m_objectBufferMapped, data, size);
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

void GPUBuffer::createObjectBuffer(size_t maxObjects)
{
	m_objectBufferSize *= maxObjects;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_objectBufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_objectBuffer, &m_objectAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create object SSBO");
	}
	std::cout << "Object SSBO created successfully (" << maxObjects << " objects)" << std::endl;

	// Get mapped pointer
	VmaAllocationInfo allocInfoDetails{};
	vmaGetAllocationInfo(m_context.getAllocator(), m_objectAllocation, &allocInfoDetails);
	m_objectBufferMapped = allocInfoDetails.pMappedData;

	nameObject(m_context.getDevice(), m_objectBuffer, "ObjectBuffer_SSBO");
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
