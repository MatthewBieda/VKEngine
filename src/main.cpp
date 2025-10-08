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
#include "GPUImage.hpp" // TextureImage, DepthImage, MSAAImage, Skybox?
#include "DescriptorManager.hpp" // Bindless descriptors
#include "Pipeline.hpp" // Shaders, pipeline layout, pipeline
#include "Sync.hpp" // Semaphores & Fences
#include "Vertex.hpp" // Vertex definiton
#include "Utils.hpp" // Helper functions
#include "ImGuiOverlay.hpp" // User Interface
#include "Camera.hpp" // Free camera
#include "Lights.hpp" // Light types

struct PushConstants
{
	glm::mat4 view{};
	glm::mat4 proj{};
	glm::vec3 cameraPos;
	uint32_t enableDirectionalLight;
	uint32_t enablePointLights;
} pc;

struct LightingData
{
	DirectionalLight dirLight;
	int numPointLights;
	alignas(16) PointLight pointsLights[16];
} lights;

struct ObjectData
{
	glm::mat4 model{};
};
uint32_t maxObjects = 16;
std::vector<ObjectData> objectData(maxObjects);

struct AppState 
{
	uint32_t windowWidth = 1920;
	uint32_t windowHeight = 1080;
	bool framebufferResized = false;
	bool cursorEnabled = false;
	bool spacePressedLastFrame = false;
	bool firstMouse = true;
} appState;

Camera camera;

const std::string TEXTURE_PATH = "../Textures/viking_room.png";

std::vector<Vertex> vertices{};
std::vector<uint32_t> indices{};

GLFWwindow* createWindow(AppState& appState);
static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window, float deltaTime);

void loadModel(const std::string& modelPath);
void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData);
void setupLighting(GPUBuffer& buffer, LightingData& lights);
void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image);

int main()
{
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	loadModel("../Models/viking_room.obj");

	GLFWwindow* window = createWindow(appState);
	VulkanContext context(window);
	Swapchain swapchain(window, context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);

	GPUBuffer buffer(context, commands, vertices, indices, sizeof(ObjectData));
	buffer.createObjectBuffer(maxObjects);
	setupSceneObjects(buffer, objectData);
	buffer.createLightingBuffer(sizeof(LightingData));
	setupLighting(buffer, lights);

	GPUImage image(context, commands, TEXTURE_PATH, swapchain.getExtent());
	image.createDepthImage(swapchain.getExtent().width, swapchain.getExtent().height);
	image.createMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());

	DescriptorManager descriptors(context, buffer, image);
	Pipeline pipeline(context, swapchain, descriptors, "../Shaders/vert.spv", "../Shaders/frag.spv", image.getDepthFormat());
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);

	ImGuiOverlay imgui;
	imgui.init(window, context, descriptors, swapchain.getFormat(), swapchain.getImageCount(), image.getMSAASamples());

	VkDebugUtilsLabelEXT cmdLabel = makeLabel("Command List: ", 0.2f, 0.2f, 0.8f);

	// Pre-render loop struct initialization
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT; // Enable resolve
	colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // For presentation
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	VkRenderingAttachmentInfo imguiColorAttachment{};
	imguiColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	imguiColorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	imguiColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	imguiColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo imguiRenderingInfo{};
	imguiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	imguiRenderingInfo.layerCount = 1;
	imguiRenderingInfo.colorAttachmentCount = 1;
	imguiRenderingInfo.pColorAttachments = &imguiColorAttachment;

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
	preRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	preRenderBarrier.subresourceRange.baseMipLevel = 0;
	preRenderBarrier.subresourceRange.levelCount = 1;
	preRenderBarrier.subresourceRange.baseArrayLayer = 0;
	preRenderBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo preDepInfo{};
	preDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	preDepInfo.imageMemoryBarrierCount = 1;
	preDepInfo.pImageMemoryBarriers = &preRenderBarrier;

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
	postRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	postRenderBarrier.subresourceRange.baseMipLevel = 0;
	postRenderBarrier.subresourceRange.levelCount = 1;
	postRenderBarrier.subresourceRange.baseArrayLayer = 0;
	postRenderBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo postDepInfo{};
	postDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	postDepInfo.imageMemoryBarrierCount = 1;
	postDepInfo.pImageMemoryBarriers = &postRenderBarrier;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.signalSemaphoreInfoCount = 1;

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkCommandBufferSubmitInfo cmdBufferInfo{};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.swapchainCount = 1;

	// Main Render Loop
	double lastTime{};
	while (!glfwWindowShouldClose(window))
	{
		// Delta time
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
		vkBeginCommandBuffer(cmd, &beginInfo);
		vkCmdBeginDebugUtilsLabelEXT(cmd, &cmdLabel);

		// Transition swapchain to attachment
		preRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		vkCmdPipelineBarrier2(cmd, &preDepInfo);

		// Color Attachment - Render to MSAA target, resolve to Swapchain
		colorAttachment.imageView = image.getMSAAColorImageView(); // Render to MSAA target
		colorAttachment.resolveImageView = swapchain.getSwapchainImageView(imageIndex); // Resolve to swapchain
		const glm::vec3& cc = imgui.clearColor;
		colorAttachment.clearValue = { {cc.r, cc.g, cc.b, 1.0f} };

		// Depth Attachment - MSAA depth buffer
		depthAttachment.imageView = image.getDepthImageView(); 
		depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = swapchain.getExtent();

		// Main render pass
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

		pc.view = camera.GetViewMatrix();
		pc.proj = glm::perspective(glm::radians(camera.Zoom),
			(float)appState.windowWidth / (float)appState.windowHeight,
			0.1f, 50.0f);
		// Flip Y scaling factor for Vulkan compatibility with GLM
		pc.proj[1][1] *= -1;
		pc.cameraPos = camera.Position;
		pc.enableDirectionalLight = imgui.enableDirectionalLight ? 1 : 0;
		pc.enablePointLights = imgui.enablePointLights ? 1 : 0;

		vkCmdPushConstants(cmd, pipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), maxObjects, 0, 0, 0);

		vkCmdEndRendering(cmd);

		// ImGui pass
		imgui.render();

		imguiColorAttachment.imageView = swapchain.getSwapchainImageView(imageIndex);
		imguiRenderingInfo.renderArea.offset = { 0, 0 };
		imguiRenderingInfo.renderArea.extent = swapchain.getExtent();

		vkCmdBeginRendering(cmd, &imguiRenderingInfo);
		imgui.recordCommands(cmd);
		vkCmdEndRendering(cmd);

		// Transition to present
		postRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		vkCmdPipelineBarrier2(cmd, &postDepInfo);

		vkCmdEndDebugUtilsLabelEXT(cmd);
		vkEndCommandBuffer(cmd);

		// Submit
		cmdBufferInfo.commandBuffer = cmd;
		waitSemaphoreInfo.semaphore = sync.getImageAvailableSemaphore(currentFrame);
		signalSemaphoreInfo.semaphore = sync.getRenderFinishedSemaphore(imageIndex);
		vkQueueSubmit2(context.getGraphicsQueue(), 1, &submitInfo, sync.getInFlightFence(currentFrame));

		// Present
		VkSemaphore presentWaitSemaphores[] = { sync.getRenderFinishedSemaphore(imageIndex) };
		presentInfo.pWaitSemaphores = presentWaitSemaphores;
		VkSwapchainKHR swapChains[] = { swapchain.getSwapchain() };
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

void loadModel(const std::string& modelPath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) 
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

void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData)
{
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
}

void setupLighting(GPUBuffer& buffer, LightingData& lights)
{
	lights.dirLight.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	lights.dirLight.color = glm::vec4(1.0f);

	lights.pointsLights[0].position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
	lights.pointsLights[0].color = glm::vec4(5.0f, 0.0f, 0.0f, 1.0f);
	lights.pointsLights[0].radius = 5.0f;

	lights.pointsLights[1].position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
	lights.pointsLights[1].color = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f);
	lights.pointsLights[1].radius = 5.0f;

	lights.numPointLights = 2;
	buffer.updateLightingBuffer(&lights, sizeof(LightingData));
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
	if (appState.cursorEnabled)
	{
		return;
	}

	static double lastX = xpos;
	static double lastY = ypos;

	if (appState.firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		appState.firstMouse = false;
	}

	float xOffset = static_cast<float>(xpos - lastX);
	float yOffset = static_cast<float>(lastY - ypos); // Reversed: y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xOffset, yOffset);
}

void processInput(GLFWwindow* window, const float deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}
	
	bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
	if (spacePressed && !appState.spacePressedLastFrame)
	{
		appState.cursorEnabled = !appState.cursorEnabled;
		if (appState.cursorEnabled)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			appState.firstMouse = true;
		}
	}
	appState.spacePressedLastFrame = spacePressed;

	if (!appState.cursorEnabled )
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

GLFWwindow* createWindow(AppState& appState)
{
	if (!glfwInit())
	{
		throw std::runtime_error("Failed to start GLFW!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(
		appState.windowWidth,
		appState.windowHeight,
		"VKEngine",
		nullptr,
		nullptr
	);

	glfwSetScrollCallback(window, scrollCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowUserPointer(window, &appState);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	return window;
}
