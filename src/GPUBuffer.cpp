#include "GPUBuffer.hpp"

#include "VulkanContext.hpp"
#include "Commands.hpp"
#include <stdexcept>

GPUBuffer::GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint16_t> indices)
	: m_context(context), m_commands(commands)
{
	createVertexBuffer(vertices);
	createIndexBuffer(indices);
}

GPUBuffer::~GPUBuffer()
{
	vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAllocation);
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

	// copy staging -> GPU
	copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

	// cleanup
	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);
}

void GPUBuffer::createIndexBuffer(const std::vector<uint16_t>& indices)
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
		throw std::runtime_error("Failed to create staging buffer");
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
		throw std::runtime_error("Failed to create index buffer");
	};

	// copy staging -> GPU
	copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

	// cleanup
	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);
}


void GPUBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_commands.getCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_context.getGraphicsQueue());

	vkFreeCommandBuffers(m_context.getDevice(), m_commands.getCommandPool(), 1, &commandBuffer);
}