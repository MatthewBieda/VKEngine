#include "GPUBuffer.hpp"

#include "VulkanContext.hpp"
#include "Commands.hpp"
#include <stdexcept>

GPUBuffer::GPUBuffer(VulkanContext& context, Commands& commands, const std::vector<Vertex>& vertices, const std::vector<uint16_t> indices, uint32_t maxFramesInFlight, VkDeviceSize uniformBufferSize)
	: m_context(context), m_commands(commands), m_maxFramesInFlight(maxFramesInFlight), 
	  m_uniformBuffers(maxFramesInFlight), m_uniformAllocations(maxFramesInFlight), m_uniformBuffersMapped(maxFramesInFlight)
{
	createVertexBuffer(vertices);
	createIndexBuffer(indices);
	createUniformBuffers(uniformBufferSize);
}

GPUBuffer::~GPUBuffer()
{
	vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAllocation);
	vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAllocation);

	for (size_t i = 0; i < m_maxFramesInFlight; ++i)
	{
		vmaDestroyBuffer(m_context.getAllocator(), m_uniformBuffers[i], m_uniformAllocations[i]);
	}
}

void GPUBuffer::updateUniformBuffer(size_t frameIndex, const void* data, size_t size)
{
	if (frameIndex >= m_maxFramesInFlight)
	{
		throw std::runtime_error("Frame index out of bounds!");
	}

	// Since we're using persistent mapping, just copy directly
	memcpy(m_uniformBuffersMapped[frameIndex], data, size);
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

	// copy staging -> GPU
	copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

	// cleanup
	vmaDestroyBuffer(m_context.getAllocator(), stagingBuffer, stagingAllocation);
}

void GPUBuffer::createUniformBuffers(VkDeviceSize bufferSize)
{
	for (size_t i = 0; i < m_maxFramesInFlight; ++i)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo, &m_uniformBuffers[i], &m_uniformAllocations[i], nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Uniform Buffer");
		}

		// Get mapped pointer
		VmaAllocationInfo allocInfoDetails{};
		vmaGetAllocationInfo(m_context.getAllocator(), m_uniformAllocations[i], &allocInfoDetails);
		m_uniformBuffersMapped[i] = allocInfoDetails.pMappedData;
	}
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