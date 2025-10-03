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
#include "Camera.hpp" // Free camera
#include "Lights.hpp" // Light types

// Per-object data
struct ObjectData
{
	glm::mat4 model{};
};
uint32_t maxObjects = 16;
std::vector<ObjectData> objectData(maxObjects);

// MVP Matrix
struct CameraData
{
	glm::mat4 view{};
	glm::mat4 proj{};
} cameraData;

struct AppState 
{
	uint32_t windowWidth = 1920;
	uint32_t windowHeight = 1080;
	bool framebufferResized = false;
} appState;

const std::string MODEL_PATH = "../Models/viking_room.obj";
const std::string TEXTURE_PATH = "../Textures/viking_room.png";

std::vector<Vertex> vertices{};
std::vector<uint32_t> indices{};

// Create camera
Camera camera;
bool cursorEnabled = false;
bool spacePressedLastFrame = false;
bool firstMouse = true;

struct LightingData
{
	DirectionalLight dirLight;
	int numPointLights;
	alignas(16) PointLight pointsLights[16];
} lights;

void loadModel();
void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image);

static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window, float deltaTime);

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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	GLFWwindow* window = glfwCreateWindow(appState.windowWidth, appState.windowHeight, "VKEngine", nullptr, nullptr);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set the app state as user pointer so callback can access it
	glfwSetWindowUserPointer(window, &appState);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	loadModel();

	VulkanContext context(window);
	Swapchain swapchain(window, context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);
	GPUBuffer buffer(context, commands, vertices, indices, sizeof(ObjectData));

	// Create object data SSBO
	buffer.createObjectBuffer(maxObjects);

	const int gridSizeX = 4;
	const int gridSizeY = 4;
	const float spacing = 2.0f;

	const float halfWidth = (gridSizeX - 1) * spacing * 0.5f;
	const float halfHeight = (gridSizeY - 1) * spacing * 0.5f;

	size_t objIndex = 0;
	for (int y = 0; y < gridSizeY; ++y) {
		for (int x = 0; x < gridSizeX; ++x) {
			// "World-space" position centered at origin
			glm::vec3 pos(
				x * spacing - halfWidth,
				0.0f,
				y * spacing - halfHeight
			);

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, pos);
			model = glm::rotate(model, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(270.0f), glm::vec3(0.0f, 0.0f, 1.0f));

			objectData[objIndex++].model = model;
		}
	}

	// Upload all matrices to the GPU
	buffer.updateObjectBuffer(objectData.data(), objectData.size() * sizeof(ObjectData));

	// Create lighting buffer and upload data
	buffer.createLightingBuffer(sizeof(LightingData));

	// Directional light
	lights.dirLight.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	lights.dirLight.color = glm::vec4(1.0f);

	// Point lights
	lights.pointsLights[0].position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
	lights.pointsLights[0].color = glm::vec4(5.0f, 0.0f, 0.0f, 1.0f);
	lights.pointsLights[0].radius = 5.0f;

	lights.pointsLights[1].position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
	lights.pointsLights[1].color = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f);
	lights.pointsLights[1].radius = 5.0f;
	lights.numPointLights = 2;
	buffer.updateLightingBuffer(&lights, sizeof(LightingData));

	GPUImage image(context, commands, TEXTURE_PATH, swapchain.getExtent());

	// Create depth and MSAA render targets
	image.createDepthImage(swapchain.getExtent().width, swapchain.getExtent().height);
	image.createMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());

	DescriptorManager descriptors(context, buffer, image);
	Pipeline pipeline(context, swapchain, descriptors, "../Shaders/vert.spv", "../Shaders/frag.spv", image.getDepthFormat());
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);

	std::cout << "Loaded " << vertices.size() << " vertices and " << indices.size() << " indices.\n";

	// Initialize ImGui using existing resources
	ImGuiOverlay imgui;
	imgui.init(window, context, descriptors, swapchain.getFormat(), swapchain.getImageCount(), image.getMSAASamples());

	// Create label
	VkDebugUtilsLabelEXT cmdLabel = makeLabel("Command List: ", 0.2f, 0.2f, 0.8f);

	double lastTime{};
	while (!glfwWindowShouldClose(window))
	{
		// calculate delta time
		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - lastTime);
		lastTime = currentTime;

		// Poll events (mouse callbacks)
		glfwPollEvents();
		processInput(window, deltaTime);

		imgui.newFrame();
		imgui.drawUI();

		// Update Lighting data
		static float t = 0.0f;
		t += deltaTime;

		// Light 1: Large circle, clockwise
		float radius1 = 3.0f;
		lights.pointsLights[0].position = glm::vec4(
			radius1 * cos(t),
			2.0f,
			radius1 * sin(t),
			1.0f
		);

		// Light 2: Smaller circle, counterclockwise
		float radius2 = 1.0f;
		lights.pointsLights[1].position = glm::vec4(
			radius2 * cos(-t * 1.2f),
			2.0f,
			radius2 * sin(-t * 1.2f),
			1.0f
		);

		buffer.updateLightingBuffer(&lights, sizeof(LightingData));

		// Wait for previous frame to finish
		vkWaitForFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame), VK_TRUE, UINT64_MAX);

		// Acquire next swapchain image
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(context.getDevice(), swapchain.getSwapchain(), UINT64_MAX, sync.getImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			swapchain.recreateSwapchain();
			continue;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swapchain image!");
		}

		// Only reset the fence if we are submitting work
		vkResetFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame));

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
		VkCullModeFlags cullMode = imgui.enableBackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;

		// Bind pipeline, set dynamic state, bind buffers & descriptors, issue draw
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
		pipeline.setViewport(cmd, viewport);
		pipeline.setScissor(cmd, scissor);
		pipeline.setDepthTest(cmd, imgui.enableDepthTest);
		pipeline.setPolygonMode(cmd, polygonMode);
		pipeline.setCullMode(cmd, cullMode);

		VkBuffer vertexBuffers[] = { buffer.getVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		VkDescriptorSet set = descriptors.getDescriptorSet();
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getLayout(), 0, 1, &set, 0, nullptr);

		cameraData.view = camera.GetViewMatrix();
		cameraData.proj = glm::perspective(glm::radians(camera.Zoom),
			(float)appState.windowWidth / (float)appState.windowHeight,
			0.1f, 50.0f);
		// Flip Y scaling factor for Vulkan compatibility with GLM
		cameraData.proj[1][1] *= -1;

		vkCmdPushConstants(cmd, pipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cameraData), &cameraData);

		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), maxObjects, 0, 0, 0);

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

		result = vkQueuePresentKHR(context.getGraphicsQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || appState.framebufferResized)
		{
			appState.framebufferResized = false;
			recreateSwapchainResources(context, swapchain, image);

			appState.windowWidth = swapchain.getExtent().width;
			appState.windowHeight = swapchain.getExtent().height;
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swapchain image!");
		}

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

			vertex.normal =
			{
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			vertex.texCoord = 
			{
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}

void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image)
{
	swapchain.recreateSwapchain();

	// Recreate extent-dependent resources
	image.recreateDepthImage(swapchain.getExtent().width, swapchain.getExtent().height);
	image.recreateMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	AppState* appState = reinterpret_cast<AppState*>(glfwGetWindowUserPointer(window));
	appState->framebufferResized = true;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (cursorEnabled)
	{
		return;
	}

	static double lastX = xpos;
	static double lastY = ypos;

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xOffset = static_cast<float>(xpos - lastX);
	float yOffset = static_cast<float>(lastY - ypos); // Reversed: y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xOffset, yOffset);
}

void processInput(GLFWwindow* window, float deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}
	
	bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
	if (spacePressed && !spacePressedLastFrame)
	{
		cursorEnabled = !cursorEnabled;
		if (cursorEnabled)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			firstMouse = true;
		}
	}
	spacePressedLastFrame = spacePressed;

	if (!cursorEnabled)
	{
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			camera.ProcessKeyboard(CameraMovement::FORWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			camera.ProcessKeyboard(CameraMovement::BACKWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.ProcessKeyboard(CameraMovement::LEFT, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.ProcessKeyboard(CameraMovement::RIGHT, deltaTime);
	}
}
