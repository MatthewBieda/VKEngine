#include <iostream>
#include <fstream>
#include <vector>
#include <array>

#include "glfw3.h"

#define GLM_FORCE_RADIANS
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "chrono"

#include "VulkanContext.hpp" // Instance, device, surface, debug messenger
#include "Swapchain.hpp" // Swapchain, image views
#include "Pipeline.hpp" // Shaders, pipeline layout, pipeline
#include "Commands.hpp" // Command pool & Command buffers
#include "DescriptorManager.hpp"   // Bindless textures, push descriptors
#include "Sync.hpp" // Semaphores & Fences
#include "GPUBuffer.hpp" // Vertex, index, uniform, storage buffers
#include "GPUImage.hpp"  // Image + ImageView + Sampler
#include "Vertex.hpp" // Vertex definiton

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f }},
	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f }},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

// MVP Matrix
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
} UBO;

uint32_t windowWidth = 1920;
uint32_t windowHeight = 1080;

void updateUniformBuffer(uint32_t currentFrame, GPUBuffer& buffer);

// Only init + run loop
int main()
{
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	if (!glfwInit())
	{
		std::cerr << "Failed to initailize GLFW!" << std::endl;
		return -1;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "VKEngine", nullptr, nullptr);

	VulkanContext context(window);
	Swapchain swapchain(context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);
	GPUBuffer buffer(context, commands, vertices, indices, MAX_FRAMES_IN_FLIGHT, sizeof(UBO));
	GPUImage image(context, commands, "../Textures/ice.jpg");
	DescriptorManager descriptors(context, buffer, image, MAX_FRAMES_IN_FLIGHT, sizeof(UBO));\
	Pipeline pipeline(context, swapchain, descriptors, "../Shaders/vert.spv", "../Shaders/frag.spv");
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Wait for previous frame to finish
		vkWaitForFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame), VK_TRUE, UINT64_MAX);
		vkResetFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame));

		// Acquire next swapchain image
		uint32_t imageIndex;
		vkAcquireNextImageKHR(context.getDevice(), swapchain.getSwapchain(), UINT64_MAX, sync.getImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

		// Reset command buffer
		vkResetCommandBuffer(commands.getCommandBuffer(currentFrame), 0);

		// Record commands
		VkCommandBuffer cmd = commands.getCommandBuffer(currentFrame);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(cmd, &beginInfo);

		// Transition swapchain image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
		VkImageMemoryBarrier2 preRenderBarrier{};
		preRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		preRenderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		preRenderBarrier.srcAccessMask = 0;
		preRenderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		preRenderBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		preRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		preRenderBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		preRenderBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preRenderBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		preRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		preRenderBarrier.subresourceRange.baseMipLevel = 0;
		preRenderBarrier.subresourceRange.levelCount = 1;
		preRenderBarrier.subresourceRange.baseArrayLayer = 0;
		preRenderBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo preDepInfo{};
		preDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		preDepInfo.imageMemoryBarrierCount = 1;
		preDepInfo.pImageMemoryBarriers = &preRenderBarrier;

		vkCmdPipelineBarrier2(cmd, &preDepInfo);

		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = swapchain.getSwapchainImageView(imageIndex);
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 1.0f} };

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = swapchain.getExtent();
		renderingInfo.layerCount = 1;  // <- required
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		// Start dynamic rendering
		vkCmdBeginRendering(cmd, &renderingInfo);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapchain.getExtent().width;
		viewport.height = (float)swapchain.getExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchain.getExtent();

		// Bind pipeline, set dynamic state, issue draw
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
		pipeline.setViewport(cmd, viewport);
		pipeline.setScissor(cmd, scissor);

		VkBuffer vertexBuffers[] = { buffer.getVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

		VkDescriptorSet currDescriptorSet = descriptors.getDescriptorSet(currentFrame);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &currDescriptorSet, 0, nullptr);

		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		// End dynamic rendering
		vkCmdEndRendering(cmd);

		// Transition swapchain image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
		VkImageMemoryBarrier2 postRenderBarrier{};
		postRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		postRenderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		postRenderBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		postRenderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		postRenderBarrier.dstAccessMask = 0;
		postRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		postRenderBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		postRenderBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postRenderBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		postRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		postRenderBarrier.subresourceRange.baseMipLevel = 0;
		postRenderBarrier.subresourceRange.levelCount = 1;
		postRenderBarrier.subresourceRange.baseArrayLayer = 0;
		postRenderBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo postDepInfo{};
		postDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		postDepInfo.imageMemoryBarrierCount = 1;
		postDepInfo.pImageMemoryBarriers = &postRenderBarrier;

		vkCmdPipelineBarrier2(commands.getCommandBuffer(currentFrame), &postDepInfo);

		vkEndCommandBuffer(commands.getCommandBuffer(currentFrame));

		updateUniformBuffer(currentFrame, buffer);

		// Submit
		VkSubmitInfo2 submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

		VkSemaphoreSubmitInfo waitSemaphoreInfo{};
		waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitSemaphoreInfo.semaphore = sync.getImageAvailableSemaphore(currentFrame);
		waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkCommandBufferSubmitInfo cmdBufferInfo{};
		cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmdBufferInfo.commandBuffer = commands.getCommandBuffer(currentFrame);

		VkSemaphoreSubmitInfo signalSemaphoreInfo{};
		signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalSemaphoreInfo.semaphore = sync.getRenderFinishedSemaphore(imageIndex);
		signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		submitInfo.waitSemaphoreInfoCount = 1;
		submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &cmdBufferInfo;
		submitInfo.signalSemaphoreInfoCount = 1;
		submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

		vkQueueSubmit2(context.getGraphicsQueue(), 1, &submitInfo, sync.getInFlightFence(currentFrame));

		// Present
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		VkSemaphore presentWaitSemaphores[] = { sync.getRenderFinishedSemaphore(imageIndex)};
		presentInfo.pWaitSemaphores = presentWaitSemaphores;
		VkSwapchainKHR swapChains[] = { swapchain.getSwapchain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		vkQueuePresentKHR(context.getGraphicsQueue(), &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// Cleanup
	// Wait for device to finish work
	vkDeviceWaitIdle(context.getDevice());

	// Destroy the GLFW window and terminate GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void updateUniformBuffer(uint32_t currentFrame, GPUBuffer& buffer)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UBO.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	UBO.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	UBO.proj = glm::perspective(glm::radians(45.0f), (float)windowWidth / (float)windowHeight, 0.1f, 10.0f);
	// Flip Y scaling factor for Vulkan compatibility with GLM
	UBO.proj[1][1] *= -1;

	buffer.updateUniformBuffer(currentFrame, &UBO, sizeof(UBO));
}