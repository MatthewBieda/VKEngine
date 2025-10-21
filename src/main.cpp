#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef APIENTRY

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <filesystem>

#include "glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define TINYOBJLOADER_IMPLEMENTATION

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "chrono"
#include <tiny_obj_loader.h>

#include "soloud.h"
#include "soloud_wav.h"

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

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;

// game physics test
struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;
};

inline BoundingBox computeBoundingBox(const glm::vec3& center, float size = 0.5f)
{
	float halfExtent = size / 2.0f;
	return
	{
		center - glm::vec3(halfExtent),
		center + glm::vec3(halfExtent)
	};
}

bool AABBIntersection(const BoundingBox& a, const BoundingBox& b)
{
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
		   (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
		   (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

// Track collected rings & game state
int ringsCollected = 0;
constexpr int totalRings = 4;

// game audio test
// Soloud test
SoLoud::Soloud gSoLoud; // SoLoud engine
SoLoud::Wav gWave; // One wave file
SoLoud::Wav gWave2; // One wave file

enum class GameState
{
	StartScreen,
	Playing,
	GameWon
};

GameState gameState = GameState::StartScreen;

struct PushConstants
{
	glm::mat4 view{};
	glm::mat4 proj{};
	alignas(16) glm::vec3 cameraPos{};
	uint32_t enableDirectionalLight = 1;
	uint32_t enablePointLights = 1;
	uint32_t enableAlphaTest = 1;
	uint32_t diffuseTextureIndex = 0;

	float reflectionStrength = 0.0f;
} pc;

struct LightingData
{
	DirectionalLight dirLight;
	int numPointLights;
	alignas(16) PointLight pointLights[128];
} lights;

struct Submesh
{
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t materialIndex;
	uint32_t padding = 0;
};

struct Mesh
{
	uint32_t vertexOffset;
	uint32_t vertexCount;
	uint32_t submeshOffset;
	uint32_t submeshCount;
};

struct Material
{
	uint32_t albedoTexture;
	uint32_t normalTexture;
	uint32_t specularTexture;

	uint32_t twosided; // cull none, otherwise cull backface
	uint32_t alphatest; // for foliage
	uint32_t alphablending; // for windows

	float shininess;
	float reflectionStrength;
	float specularStrength;
	float alphaThreshold;
};

struct ObjectData
{
	glm::mat4 model;
	uint32_t meshIndex;
	uint32_t collected = 0;
	uint32_t padding2 = 0;
	uint32_t padding3 = 0;
};
std::vector<ObjectData> objectData{};

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

std::vector<Vertex> allVertices{};
std::vector<uint32_t> allIndices{};
std::vector<Mesh> allMeshes{};
std::vector<Submesh> allSubmeshes{};
std::vector<Material> allMaterials{};

GLFWwindow* createWindow(AppState& appState);
static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, float deltaTime);

uint32_t loadModel(const std::string& modelPath, GPUImage& imageClass);

enum class MeshType
{
	LightCaster,
	Sponza,
	Cube,
	Ring
};

void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData);
void setupLighting(GPUBuffer& buffer, LightingData& lights);
void updateLighting(LightingData& lights, float deltaTime);
void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, GLFWwindow* window, float deltaTime);
void updatePhysics(std::vector<ObjectData>& objectData);

void resetGame();

void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image);

int main()
{
	// Initialize GLFW and SoLoud
	GLFWwindow* window = createWindow(appState);
	gSoLoud.init();
	gWave.load("../Audio/shadowing.wav");
	gWave2.load("../Audio/retro8.wav");

	// Initialize Vulkan core
	VulkanContext context(window);
	Swapchain swapchain(window, context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);

	// Create GPU Image resources
	GPUImage image(context, commands);
	image.createDepthImage(swapchain.getExtent().width, swapchain.getExtent().height);
	image.createMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());

	// Load textures and models
	std::array<std::string, 6> skyBoxFaces = {
		"../Textures/Skyboxes/YokohamaCity/posx.jpg",
		"../Textures/Skyboxes/YokohamaCity/negx.jpg",
		"../Textures/Skyboxes/YokohamaCity/posy.jpg",
		"../Textures/Skyboxes/YokohamaCity/negy.jpg",
		"../Textures/Skyboxes/YokohamaCity/posz.jpg",
		"../Textures/Skyboxes/YokohamaCity/negz.jpg",
	};
	image.createCubemap(skyBoxFaces);

	uint32_t lightCaster = loadModel("../Models/LightCaster/lightCaster.obj", image);
	uint32_t sponza = loadModel("../Models/Sponza/sponza.obj", image);
	uint32_t cube = loadModel("../Models/Cube/cube.obj", image);
	uint32_t ring = loadModel("../Models/Ring/ring.obj", image);

	// Create buffers and populate scene
	GPUBuffer buffer(context, commands, allVertices, allIndices, sizeof(ObjectData), MAX_FRAMES_IN_FLIGHT);
	setupLighting(buffer, lights);
	setupSceneObjects(buffer, objectData);

	// Setup descriptors and pipelines
	DescriptorManager descriptors(context, buffer, image);
	descriptors.updateTextureArray(image.getTextureViews(), image.getSampler());

	Pipeline scenePipeline(context, swapchain, descriptors, sizeof(PushConstants), "../Shaders/vert.spv", "../Shaders/frag.spv", image.getDepthFormat(), PipelineType::Scene);
	Pipeline skyboxPipeline(context, swapchain, descriptors, sizeof(PushConstants), "../Shaders/skyboxvert.spv", "../Shaders/skyboxfrag.spv", image.getDepthFormat(), PipelineType::Skybox);
	Pipeline transparentPipeline(context, swapchain, descriptors, sizeof(PushConstants), "../Shaders/vert.spv", "../Shaders/frag.spv", image.getDepthFormat(), PipelineType::Transparent);

	// Setup syncronization and UI
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);
	ImGuiOverlay imgui;
	imgui.init(window, context, descriptors, swapchain.getFormat(), swapchain.getImageCount(), image.getMSAASamples());

	//Debug labels
	VkDebugUtilsLabelEXT opaquePassLabel = makeLabel("Opaque Pass", 0.0f, 1.0f, 0.0f);
	VkDebugUtilsLabelEXT skyboxPassLabel = makeLabel("Skybox Pass", 0.3f, 0.7f, 1.0f);
	VkDebugUtilsLabelEXT transparentPassLabel = makeLabel("Transparent Pass", 1.0f, 0.5f, 0.0f);
	VkDebugUtilsLabelEXT imguiPassLabel = makeLabel("ImGui Pass", 1.0f, 0.0f, 1.0f);

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

	VkDescriptorSet set = descriptors.getDescriptorSet();

	// Group objects by mesh
	std::unordered_map<uint32_t, std::vector<uint32_t>> objectsByMesh;	
	for (uint32_t i = 0; i < objectData.size(); ++i)
	{
		objectsByMesh[objectData[i].meshIndex].push_back(i);
	}

	// Create draw command lists for opaque and transparent objects
	struct DrawCommand
	{
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		int32_t vertexOffset;
		uint32_t firstInstance;

		Material material;
		VkCullModeFlagBits cullMode;
	};

	std::vector<std::vector<DrawCommand>> opaqueDrawsByMaterial(allMaterials.size());
	std::vector<std::vector<DrawCommand>> transparentDrawsByMaterial(allMaterials.size());

	// Build draw command list grouped by material
	for (const auto& [meshIndex, instanceIndices] : objectsByMesh)
	{
		const Mesh& mesh = allMeshes[meshIndex];

		// Loop over submeshes within this mesh
		for (uint32_t submeshIdx = 0; submeshIdx < mesh.submeshCount; ++submeshIdx)
		{
			const Submesh& submesh = allSubmeshes[mesh.submeshOffset + submeshIdx];
			const Material& material = allMaterials[submesh.materialIndex];

			DrawCommand cmd{};
			cmd.indexCount = submesh.indexCount;
			cmd.instanceCount = static_cast<uint32_t>(instanceIndices.size());
			cmd.firstIndex = submesh.indexOffset;
			cmd.vertexOffset = static_cast<uint32_t>(mesh.vertexOffset);
			cmd.firstInstance = instanceIndices.front();
			cmd.material = material;
			cmd.cullMode = (material.twosided == 1) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

			// Split opaque vs transparent at build time
			if (material.alphablending == 1)
			{
				transparentDrawsByMaterial[submesh.materialIndex].push_back(cmd);
			}
			else
			{
				opaqueDrawsByMaterial[submesh.materialIndex].push_back(cmd);
			}
		}
	}

	// Start playing background music
	gSoLoud.play(gWave, 0.3f, 0.0f, 0, 0);

	double lastTime{};
	while (!glfwWindowShouldClose(window))
	{
		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - lastTime);
		lastTime = currentTime;

		// Update Input
		glfwPollEvents();
		processInput(window, deltaTime);

		// Update UI
		imgui.newFrame();
		//imgui.drawUI();

		if (gameState == GameState::StartScreen)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2((float)appState.windowWidth, (float)appState.windowHeight));
			ImGui::Begin("StartScreen", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			// Semi-transparent dark background
			drawList->AddRectFilled(
				ImVec2(0, 0),
				ImVec2((float)appState.windowWidth, (float)appState.windowHeight),
				IM_COL32(0, 0, 0, 200)
			);

			const char* title = "SPONZA ADVENTURE!";
			const char* objective = "Collect all the rings to win";
			const char* controls = "Arrow keys to move.";
			const char* start = "PRESS SPACE TO START";

			ImFont* font = ImGui::GetFont();
			float baseFontSize = ImGui::GetFontSize();

			auto drawCenteredText = [&](const char* text, float scale, float& y, ImU32 color) {
				float fontSize = baseFontSize * scale;
				// Pass the actual font size to CalcTextSize
				ImVec2 size = ImGui::CalcTextSize(text, nullptr, false, 0.0f);
				size.x *= scale; // scale manually because CalcTextSize uses font size 1x
				size.y *= scale;

				ImVec2 pos((float)appState.windowWidth / 2.0f - size.x / 2.0f, y);
				drawList->AddText(font, fontSize, pos, color, text);
				y += size.y + (scale * 10.0f);
				};


			float y = appState.windowHeight * 0.4f; // start higher
			drawCenteredText(title, 4.0f, y, IM_COL32(255, 255, 0, 255));
			drawCenteredText(objective, 2.0f, y, IM_COL32(255, 255, 255, 255));
			drawCenteredText(controls, 2.0f, y, IM_COL32(200, 200, 200, 255));
			drawCenteredText(start, 3.0f, y, IM_COL32(255, 255, 0, 255));

			ImGui::End();

			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
				gameState = GameState::Playing;
		}
		else if (gameState == GameState::Playing)
		{
			// Ring counter
			ImGui::Begin("##Collectibles");
			ImGui::SetWindowFontScale(3.0f);
			ImGui::Text("Rings Collected: %d/%d", ringsCollected, totalRings);
			ImGui::SetWindowFontScale(1.0f);
			ImGui::End();
		}
		else if (gameState == GameState::GameWon)
		{
			// Fullscreen overlay
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2((float)appState.windowWidth, (float)appState.windowHeight));
			ImGui::Begin("Game Over Overlay", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

			// Semi-transparent dark background
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(
				ImVec2(0, 0),
				ImVec2((float)appState.windowWidth, (float)appState.windowHeight),
				IM_COL32(0, 0, 0, 180) // RGBA, 180 alpha for semi-transparency
			);

			// Text lines
			const char* lines[] = {
				"YOU WIN!",
				"Press ESC to quit.",
				"Press SPACE to play again."
			};

			ImFont* font = ImGui::GetFont();
			float baseFontSize = ImGui::GetFontSize();

			auto drawCenteredText = [&](const char* text, float scale, float& y, ImU32 color) {
				float fontSize = baseFontSize * scale;
				ImVec2 size = ImGui::CalcTextSize(text);
				size.x *= scale; // scale manually because CalcTextSize uses font size 1x
				size.y *= scale;

				ImVec2 pos((float)appState.windowWidth / 2.0f - size.x / 2.0f, y);
				drawList->AddText(font, fontSize, pos, color, text);
				y += size.y + (scale * 10.0f); // vertical spacing
				};

			// Start drawing from slightly above center
			float y = appState.windowHeight * 0.4f;
			drawCenteredText(lines[0], 4.0f, y, IM_COL32(255, 255, 0, 255));  // YOU WIN!
			drawCenteredText(lines[1], 2.0f, y, IM_COL32(255, 255, 255, 255)); // Press ESC
			drawCenteredText(lines[2], 2.0f, y, IM_COL32(200, 200, 200, 255)); // Press SPACE

			ImGui::End();

			// Handle input
			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
			{
				resetGame();
			}
			if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}

		updateLighting(lights, deltaTime);
		updateObjects(objectData, lights, window, deltaTime);
		updatePhysics(objectData);

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

		vkResetFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame));
		vkResetCommandBuffer(commands.getCommandBuffer(currentFrame), 0);

		// Update GPU resources
		buffer.updateObjectBuffer(objectData.data(), objectData.size() * sizeof(ObjectData), currentFrame);
		buffer.updateLightingBuffer(&lights, sizeof(LightingData), currentFrame);

		// Record commands
		VkCommandBuffer cmd = commands.getCommandBuffer(currentFrame);
		vkBeginCommandBuffer(cmd, &beginInfo);

		// Transition swapchain to attachment
		preRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		vkCmdPipelineBarrier2(cmd, &preDepInfo);

		// Color Attachment - Render to MSAA target, resolve to Swapchain
		colorAttachment.imageView = image.getMSAAColorImageView(); // Render to MSAA target
		colorAttachment.resolveImageView = swapchain.getSwapchainImageView(imageIndex); // Resolve to swapchain

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

		// 1. Opaque Pass (depth writes ON)
		vkCmdBeginDebugUtilsLabelEXT(cmd, &opaquePassLabel);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.getPipeline());
		scenePipeline.setViewport(cmd, viewport);
		scenePipeline.setScissor(cmd, scissor);
		scenePipeline.setDepthTest(cmd, imgui.enableDepthTest);
		scenePipeline.setPolygonMode(cmd, polygonMode);

		VkBuffer vertexBuffers[] = { buffer.getVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		// Calculate dynamic offset for current frame
		std::array<uint32_t, 2> dynamicOffsets = {
			static_cast<uint32_t>(currentFrame * buffer.getAlignedObjectSize()),
			static_cast<uint32_t>(currentFrame * buffer.getAlignedLightingSize())
		};

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.getLayout(), 0, 1, &set, 2, dynamicOffsets.data());

		pc.view = camera.GetViewMatrix();
		pc.proj = glm::perspective(glm::radians(camera.Zoom),
			(float)appState.windowWidth / (float)appState.windowHeight,
			0.1f, 50.0f);
		// Flip Y scaling factor for Vulkan compatibility with GLM
		pc.proj[1][1] *= -1;
		pc.cameraPos = camera.Position;
		pc.enableDirectionalLight = imgui.enableDirectionalLight ? 1 : 0;
		pc.enablePointLights = imgui.enablePointLights ? 1 : 0;

		// Loop over meshes
		for (uint32_t matIdx = 0; matIdx < opaqueDrawsByMaterial.size(); ++matIdx)
		{
			const auto& drawCmds = opaqueDrawsByMaterial[matIdx];

			if (drawCmds.empty())
			{
				continue;
			}
			
			const Material& material = drawCmds[0].material;

			// Set cull mode (same for all submeshes of this material)
			scenePipeline.setCullMode(cmd, drawCmds[0].cullMode);

			// Set alpha test
			pc.enableAlphaTest = (material.alphatest == 1) ? 1 : 0;
			pc.diffuseTextureIndex = static_cast<int>(material.albedoTexture);
			pc.reflectionStrength = material.reflectionStrength;

			// Push constants once per material
			vkCmdPushConstants(cmd, scenePipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

			// Draw all submeshes of this material
			for (const auto& drawCmd : drawCmds)
			{
				vkCmdDrawIndexed(cmd, drawCmd.indexCount, drawCmd.instanceCount, drawCmd.firstIndex, drawCmd.vertexOffset, drawCmd.firstInstance
				);
			}
		}
		vkCmdEndDebugUtilsLabelEXT(cmd);

		// 2. Skybox pass (Depth wrties OFF, test ON)
		vkCmdBeginDebugUtilsLabelEXT(cmd, &skyboxPassLabel);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.getPipeline());
		skyboxPipeline.setViewport(cmd, viewport);
		skyboxPipeline.setScissor(cmd, scissor);
		skyboxPipeline.setDepthTest(cmd, VK_TRUE);
		skyboxPipeline.setPolygonMode(cmd, VK_POLYGON_MODE_FILL);
		skyboxPipeline.setCullMode(cmd, VK_CULL_MODE_FRONT_BIT);

		PushConstants skyboxPC = pc;
		skyboxPC.view = glm::mat4(glm::mat3(camera.GetViewMatrix())); // remove translation
		vkCmdPushConstants(cmd, skyboxPipeline.getLayout(),
						   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						   0, sizeof(PushConstants), &skyboxPC);

		vkCmdDraw(cmd, 36, 1, 0, 0);
		vkCmdEndDebugUtilsLabelEXT(cmd);

		// 3. Transparent pass (Depth writes OFF, sorted back to front)
		vkCmdBeginDebugUtilsLabelEXT(cmd, &transparentPassLabel);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline.getPipeline());

		// Struct to hold per-instance transparent draw info
		struct TransparentInstance {
			uint32_t objIndex;
			uint32_t materialIndex;
			float distanceToCamera;
		};

		// Collect all transparent objects
		std::vector<TransparentInstance> transparentObjects;

		for (uint32_t matIdx = 0; matIdx < transparentDrawsByMaterial.size(); ++matIdx)
		{
			auto& drawCommands = transparentDrawsByMaterial[matIdx];
			for (auto& cmd: drawCommands)
			{
				for (uint32_t i = 0; i < cmd.instanceCount; ++i)
				{
					uint32_t objIndex = cmd.firstInstance + i;
					glm::vec3 objPos = glm::vec3(objectData[objIndex].model[3]);

					float dist = glm::length(camera.Position - objPos);
					transparentObjects.push_back({ objIndex, matIdx, dist });
				}
			}
		}

		// Sort back to front
		std::sort(transparentObjects.begin(), transparentObjects.end(),
			[](const TransparentInstance& a, const TransparentInstance& b) {
				return a.distanceToCamera > b.distanceToCamera;
			});

		// Draw transparent objects individually
		for (const auto& inst: transparentObjects)
		{
			const auto& drawCmd = transparentDrawsByMaterial[inst.materialIndex][0];
			const auto& mat = drawCmd.material;

			pc.enableAlphaTest = mat.alphatest;
			pc.diffuseTextureIndex = static_cast<int>(mat.albedoTexture);
			pc.reflectionStrength = mat.reflectionStrength;

			vkCmdPushConstants(cmd, transparentPipeline.getLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(pc), &pc);

			// Back faces
			transparentPipeline.setCullMode(cmd, VK_CULL_MODE_FRONT_BIT);
			vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, inst.objIndex);

			// Front faces
			transparentPipeline.setCullMode(cmd, VK_CULL_MODE_BACK_BIT);
			vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, inst.objIndex);
		}

		vkCmdEndDebugUtilsLabelEXT(cmd);
		vkCmdEndRendering(cmd);

		// 4. UI pass
		vkCmdBeginDebugUtilsLabelEXT(cmd, &imguiPassLabel);
		imgui.render();

		imguiColorAttachment.imageView = swapchain.getSwapchainImageView(imageIndex);
		imguiRenderingInfo.renderArea.offset = { 0, 0 };
		imguiRenderingInfo.renderArea.extent = swapchain.getExtent();

		vkCmdBeginRendering(cmd, &imguiRenderingInfo);
		imgui.recordCommands(cmd);
		vkCmdEndDebugUtilsLabelEXT(cmd);
		vkCmdEndRendering(cmd);

		// Transition to present
		postRenderBarrier.image = swapchain.getSwapchainImage(imageIndex);
		vkCmdPipelineBarrier2(cmd, &postDepInfo);

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

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(context.getDevice());
	glfwDestroyWindow(window);
	glfwTerminate();
	gSoLoud.deinit();

	return 0;
}

uint32_t loadModel(const std::string& modelPath, GPUImage& imageClass)
{
	namespace fs = std::filesystem;

	fs::path objFilePath(modelPath);
	fs::path objDir = objFilePath.parent_path();

	tinyobj::ObjReaderConfig reader_config;
	reader_config.mtl_search_path = objDir.string();
	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(modelPath, reader_config))
	{
		if (!reader.Error().empty())
		{
			std::cerr << "TinyObjReader: " << reader.Error();
		}
		exit(1);
	}

	if (!reader.Warning().empty())
	{
		std::cout << "TinyObjReader: " << reader.Warning();
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();
	auto& materials = reader.GetMaterials();

	Mesh mesh{};
	mesh.vertexOffset = static_cast<uint32_t>(allVertices.size());
	mesh.submeshOffset = static_cast<uint32_t>(allSubmeshes.size());

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	// record where new materials will start in global array
	uint32_t baseMaterialIndex = static_cast<uint32_t>(allMaterials.size());

	// iterate shapes
	for (const auto& shape : shapes) {
		Submesh sub{};
		sub.indexOffset = static_cast<uint32_t>(allIndices.size());

		// local material index from tinyobj
		uint32_t matIndex = UINT32_MAX;
		if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
			matIndex = static_cast<uint32_t>(shape.mesh.material_ids[0]);
		}

		// convert local index to global index by adding base offset (if valid)
		if (matIndex != UINT32_MAX)
		{
			matIndex += baseMaterialIndex;
		}
		sub.materialIndex = matIndex;

		for (const tinyobj::index_t& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			if (index.normal_index >= 0) {
				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};
			}

			if (index.texcoord_index >= 0) {
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}

			//vertex.tangent = glm::vec3(0.0f);

			if (!uniqueVertices.contains(vertex)) {
				uniqueVertices[vertex] = static_cast<uint32_t>(allVertices.size() - mesh.vertexOffset);
				allVertices.push_back(vertex);
			}

			allIndices.push_back(uniqueVertices[vertex]);
		}

		sub.indexCount = static_cast<uint32_t>(allIndices.size()) - sub.indexOffset;
		allSubmeshes.push_back(sub);
	}

	mesh.vertexCount = static_cast<uint32_t>(allVertices.size() - mesh.vertexOffset);
	mesh.submeshCount = static_cast<uint32_t>(allSubmeshes.size()) - mesh.submeshOffset;

	uint32_t meshIndex = static_cast<uint32_t>(allMeshes.size());
	allMeshes.push_back(mesh);

	// Load material data
	for (const auto& mtl : materials)
	{
		Material mat{};

		if (!mtl.diffuse_texname.empty())
		{
			fs::path texPath = objDir / mtl.diffuse_texname;
			mat.albedoTexture = imageClass.loadTexture(texPath.string());
		}
		else
		{
			mat.albedoTexture = 0;
		}

		mat.normalTexture = 0;
		mat.specularTexture = 0;

		if (mtl.dissolve < 1.0f)
		{	
			// Alpha-blended transparency (e.g. glass)
			mat.twosided = 1;
			mat.alphatest = 0;
			mat.alphablending = 1;
		}
		else if (!mtl.alpha_texname.empty())
		{
			// Alpha-tested transparency (e.g. grass, fences)
			mat.twosided = 1;
			mat.alphatest = 1;
			mat.alphablending = 0;
		}
		else
		{
			// Fully opaque
			mat.twosided = 0;
			mat.alphatest = 0;
			mat.alphablending = 0;
		}

		mat.shininess = mtl.shininess;
		mat.reflectionStrength = 0.0f;
		mat.specularStrength = 0.5f;
		mat.alphaThreshold = 0.5f;

		allMaterials.push_back(mat);
	}

	std::cout << "Loaded mesh [" << meshIndex << "] with " << mesh.submeshCount
		<< " submeshes" << std::endl;

	return meshIndex;
}

void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData)
{
	glm::vec3 pos{ 0.0f, 0.0f, 0.0f };
	glm::mat4 model = { 1.0f };
	uint32_t meshIndex = 0;

	pos = { 0.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Sponza);
	objectData.push_back({ model, meshIndex });

	pos = { 0.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Cube);
	objectData.push_back({ model, meshIndex });

	pos = { -10.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	meshIndex = static_cast<uint32_t>(MeshType::Ring);
	objectData.push_back({ model, meshIndex });

	pos = { 10.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Ring);
	objectData.push_back({ model, meshIndex });

	pos = { 0.0f, 0.0f, 4.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Ring);
	objectData.push_back({ model, meshIndex });

	pos = { 0.0f, 0.0f, -5.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Ring);
	objectData.push_back({ model, meshIndex });

	buffer.createObjectBuffer(objectData.size());
	buffer.updateObjectBuffer(objectData.data(), objectData.size() * sizeof(ObjectData), currentFrame);
}

void setupLighting(GPUBuffer& buffer, LightingData& lights)
{
	lights.dirLight.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	lights.dirLight.color = glm::vec4(1.0f);

	buffer.createLightingBuffer(sizeof(LightingData));
	buffer.updateLightingBuffer(&lights, sizeof(LightingData), currentFrame);
}

void updateLighting(LightingData& lights, float deltaTime)
{

}

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, GLFWwindow* window, float deltaTime)
{
	if (gameState != GameState::GameWon)
	{
		// Define movement speed (units per second)
		float moveSpeed = 2.0f;

		// Get the cube's current position
		glm::vec3 cubePos = glm::vec3(objectData[1].model[3]);
		glm::vec3 oldPos = cubePos;

		// Arrow key movement
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		{
			cubePos.x -= moveSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		{
			cubePos.x += moveSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		{
			cubePos.z += moveSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		{
			cubePos.z -= moveSpeed * deltaTime;
		}
		objectData[1].model = glm::translate(glm::mat4(1.0f), cubePos);

		// Update camera Yaw based on movement direction
		if (glm::length(cubePos - oldPos) > 0.001f)
		{
			glm::vec3 moveDir = glm::normalize(cubePos - oldPos);
			camera.Yaw = glm::degrees(atan2(moveDir.z, moveDir.x)) - 90.0f;
		}

		// Update camera to follow cube
		camera.FollowTarget(cubePos);
	}

	if (gameState != GameState::Playing) return;
	for (int i = 2; i <= 5; ++i)
	{
		// Skip collected rings
		if (objectData[i].collected != 0)
		{
			continue;
		}

		// Make rings spin and bob
		// Spin around local Y axis
		constexpr float rotationSpeed = glm::radians(180.0f); // degrees per second
		glm::vec3 localCenter(0.0f, 0.0f, 0.0f);   // mesh center offset, adjust if needed

		// Decompose translation from obj.model
		glm::vec3 translation = glm::vec3(objectData[i].model[3]); // last column
		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationSpeed * deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));

		// Rebuild model: translate to origin, rotate, translate back
		objectData[i].model = glm::translate(glm::mat4(1.0f), translation) *
			rotation *
			glm::translate(glm::mat4(1.0f), -translation) *
			objectData[i].model;
	}
}

void updatePhysics(std::vector<ObjectData>& objectData)
{
	if (gameState != GameState::Playing) return;

	glm::vec3 cubePos = glm::vec3(objectData[1].model[3]);
	BoundingBox cubeBox = computeBoundingBox(cubePos);

	for (int i = 2; i <= 5; ++i)
	{
		if (objectData[i].collected != 0)
		{
			continue;
		}

		glm::vec3 ringPos = glm::vec3(objectData[i].model[3]);
		BoundingBox ringBox = computeBoundingBox(ringPos);

		if (AABBIntersection(cubeBox, ringBox))
		{
			gSoLoud.play(gWave2);
			objectData[i].collected = 1;
			++ringsCollected;
			std::cout << "Ring collected: Total: " << ringsCollected << std::endl;
		}
	}

	// Check for win state
	if (ringsCollected == totalRings)
	{
		gameState = GameState::GameWon;
	}
}

void resetGame()
{
	gameState = GameState::Playing;
	ringsCollected = 0;

	// reset player position
	glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
	glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
	objectData[1].model = model;

	// mark rings as uncollected
	for (int i = 2; i <= 5; ++i)
	{
		objectData[i].collected = 0;
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
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowUserPointer(window, &appState);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	return window;
}
