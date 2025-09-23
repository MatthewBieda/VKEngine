#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <unordered_map>

#include "glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define TINYOBJLOADER_IMPLEMENTATION

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "chrono"
#include <tiny_obj_loader.h>

#include "VulkanContext.hpp" // Instance, device, surface, debug messenger
#include "Swapchain.hpp" // Swapchain, image views
#include "Commands.hpp" // Command pool & Command buffers
#include "GPUBuffer.hpp" // Vertex, index, uniform, storage buffers
#include "GPUImage.hpp" // TextureImage, DepthImage, MSAAImage
#include "DescriptorManager.hpp" // Classic API for now (Pools/Sets)
#include "Pipeline.hpp" // Shaders, pipeline layout, pipeline
#include "Sync.hpp" // Semaphores & Fences
#include "Vertex.hpp" // Vertex definiton
#include "Utils.hpp" // Helper functions
#include "ImGuiOverlay.hpp" // User Interface

// MVP Matrix
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
} UBO;

uint32_t windowWidth = 1920;
uint32_t windowHeight = 1080;

const std::string MODEL_PATH = "../Models/viking_room.obj";
const std::string TEXTURE_PATH = "../Textures/viking_room.png";

std::vector<Vertex> vertices{};
std::vector<uint32_t> indices{};

void updateUniformBuffer(uint32_t currentFrame, GPUBuffer& buffer);
void loadModel();

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

	loadModel();

	VulkanContext context(window);
	Swapchain swapchain(context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);
	GPUBuffer buffer(context, commands, vertices, indices, MAX_FRAMES_IN_FLIGHT, sizeof(UBO));
	GPUImage image(context, commands, TEXTURE_PATH, swapchain.getExtent());
	image.createMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());
	DescriptorManager descriptors(context, buffer, image, MAX_FRAMES_IN_FLIGHT, sizeof(UBO));
	Pipeline pipeline(context, swapchain, descriptors, "../Shaders/vert.spv", "../Shaders/frag.spv", image.getDepthFormat());
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);

	std::cout << "Loaded " << vertices.size() << " vertices and " << indices.size() << " indices.\n";

	// Initialize ImGui using existing resources
	ImGuiOverlay imgui;
	imgui.init(window, context, descriptors, swapchain.getFormat(), swapchain.getImageCount(), image.getMSAASamples());

	// Create label
	VkDebugUtilsLabelEXT cmdLabel = makeLabel("Command List: ", 0.2f, 0.2f, 0.8f);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		imgui.newFrame();
		imgui.drawUI();

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
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmd, &beginInfo);
		vkCmdBeginDebugUtilsLabelEXT(cmd, &cmdLabel);

		// Transition swapchain image from UNDEFINED to ATTACHMENT_OPTIMAL
		VkImageMemoryBarrier2 preRenderBarrier{};
		preRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		preRenderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		preRenderBarrier.srcAccessMask = VK_ACCESS_2_NONE;
		preRenderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		preRenderBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		preRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		preRenderBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
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

		// Color Attachment - Render to MSAA target, resolve to Swapchain
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = image.getMSAAColorImageView(); // Render to MSAA target
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT; // Enable resolve
		colorAttachment.resolveImageView = swapchain.getSwapchainImageView(imageIndex); // Resolve to swapchain
		colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // For presentation
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		const glm::vec3& cc = imgui.clearColor;
		colorAttachment.clearValue = { {cc.r, cc.g, cc.b, 1.0f} };

		// Depth Attachment - MSAA depth buffer
		VkRenderingAttachmentInfo depthAttachment{};
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = image.getDepthImageView(); 
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkClearValue depthClear{};
		depthClear.depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 (far plane)
		depthAttachment.clearValue = depthClear;

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = swapchain.getExtent();
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = &depthAttachment;

		// Start dynamic rendering
		vkCmdBeginRendering(cmd, &renderingInfo);

		// Declare dynamic states
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

		VkPolygonMode polygonMode = imgui.enableWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

		// Bind pipeline, set dynamic state, bind buffers & descriptors, issue draw
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
		pipeline.setViewport(cmd, viewport);
		pipeline.setScissor(cmd, scissor);
		pipeline.setDepthTest(cmd, imgui.enableDepthTest);
		pipeline.setPolygonMode(cmd, polygonMode);

		VkBuffer vertexBuffers[] = { buffer.getVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		VkDescriptorSet currDescriptorSet = descriptors.getDescriptorSet(currentFrame);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &currDescriptorSet, 0, nullptr);

		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		// End dynamic rendering
		vkCmdEndRendering(cmd);

		// Render ImGui on top of the resolved swapchain image
		imgui.render();

		// Imgui render pass(no depth, directly on swapchain)
		VkRenderingAttachmentInfo imguiColorAttachment{};
		imguiColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		imguiColorAttachment.imageView = swapchain.getSwapchainImageView(imageIndex);
		imguiColorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		imguiColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; 
		imguiColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRenderingInfo imguiRenderingInfo{};
		imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		imguiRenderingInfo.renderArea.offset = { 0, 0 };
		imguiRenderingInfo.renderArea.extent = swapchain.getExtent();
		imguiRenderingInfo.layerCount = 1;
		imguiRenderingInfo.colorAttachmentCount = 1;
		imguiRenderingInfo.pColorAttachments = &imguiColorAttachment;

		vkCmdBeginRendering(cmd, &imguiRenderingInfo);
		imgui.recordCommands(cmd);
		vkCmdEndRendering(cmd);

		// Transition swapchain image from ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
		VkImageMemoryBarrier2 postRenderBarrier{};
		postRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		postRenderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		postRenderBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		postRenderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		postRenderBarrier.dstAccessMask = VK_ACCESS_2_NONE;
		postRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
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

		vkCmdPipelineBarrier2(cmd, &postDepInfo);

		vkCmdEndDebugUtilsLabelEXT(cmd);
		vkEndCommandBuffer(cmd);

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
		cmdBufferInfo.commandBuffer = cmd;

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
		VkSemaphore presentWaitSemaphores[] = { sync.getRenderFinishedSemaphore(imageIndex) };
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

void loadModel()
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) 
	{
		throw std::runtime_error(err);
	}

	for (const auto& shape : shapes)
	{
		for (const tinyobj::index_t& index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos =
			{
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}
