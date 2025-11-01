#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <execution> // C++ 17 parallel algorithms
#include <ranges>

#include "soloud.h"
#include "soloud_wav.h"
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
#include "DebugVertex.hpp" // Vertex data for debug AABB
#include "Utils.hpp" // Helper functions
#include "ImGuiOverlay.hpp" // User Interface
#include "Camera.hpp" // Free camera
#include "Lights.hpp" // Light types
#include "Frustum.hpp" // Camera frustum data
#include "AABB.hpp" // Axis-Aligned Bounding Boxes
#include "TangentGen.hpp" // Use MikkTSpace standard to generate tangents

// Audio test
SoLoud::Soloud gSoLoud; // SoLoud engine
SoLoud::Wav gWave; // Audio item

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;

struct PushConstants
{
	glm::mat4 view{};
	glm::mat4 proj{};
	alignas(16) glm::vec3 cameraPos{};
	uint32_t enableDirectionalLight = 1;
	uint32_t enablePointLights = 1;
	uint32_t enableAlphaTest = 1;
	uint32_t diffuseTextureIndex = 0;
	uint32_t normalTextureIndex = 0;
	uint32_t enableNormalMaps = 1;
	float reflectionStrength = 0.0f;
	uint32_t padding1 = 0;
} pc;

struct DebugPushConstants
{
	glm::mat4 view{};
	glm::mat4 proj{};
} debugPC;

struct ShadowPushConstants {
	glm::mat4 lightViewProj;
} shadowPC;

struct LightingData
{
	DirectionalLight dirLight;
	uint32_t numPointLights = 0;
	alignas(16) PointLight pointLights[128];
} lights;

struct Submesh
{
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t materialIndex;
	AABB bounds; // submesh-level AABB will be used for collision
	uint32_t padding = 0;
};

struct Mesh
{
	uint32_t vertexOffset;
	uint32_t vertexCount;
	uint32_t submeshOffset;
	uint32_t submeshCount;
	AABB bounds; // used for frustum culling
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
	uint32_t isVisible = 0; // set by frustum culling or game logic
	uint32_t padding2 = 0;
	uint32_t padding3 = 0;
};
std::vector<ObjectData> objectData{};

struct DrawCommand
{
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
	Material material;
	std::vector<uint32_t> objectIndices;
};

struct DrawLists
{
	std::vector<std::vector<DrawCommand>> opaque;
	std::vector<std::vector<DrawCommand>> transparent;
};

struct AppState 
{
	uint32_t windowWidth = 2560;
	uint32_t windowHeight = 1440;
	bool framebufferResized = false;
	bool cursorEnabled = false;
	bool spacePressedLastFrame = false;
	bool firstMouse = true;
	bool wasFreezeFrustumEnabled = false;
} appState;

Camera camera;

using Clock = std::chrono::high_resolution_clock;
using ms = std::chrono::duration<double, std::milli>;

struct ScopedTimer
{
	const char* label;
	Clock::time_point start;

	ScopedTimer(const char* lbl) : label(lbl), start(Clock::now()) {}
	~ScopedTimer()
	{
		auto end = Clock::now();
		double elapsed = std::chrono::duration_cast<ms>(end - start).count();
		std::cout << label << ": " << elapsed << " ms" << std::endl;
	}
};

std::vector<Vertex> allVertices{};
std::vector<uint32_t> allIndices{};
std::vector<Mesh> allMeshes{};
std::vector<Submesh> allSubmeshes{};
std::vector<Material> allMaterials{};

GLFWwindow* createWindow(AppState& appState);
static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window, float deltaTime);

uint32_t loadModel(const std::string& modelPath, GPUImage& imageClass);

enum class MeshType
{
	LightCaster,
	Sponza,
	AlphaTestedGrass,
	GlassWindow,
	Cube,
	BrickWall
};

void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData);
void setupLighting(GPUBuffer& buffer, LightingData& lights);
void updateLighting(LightingData& lights, float deltaTime);
void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime);
std::vector<uint32_t> performFrustumCulling(std::vector<ObjectData>& objectData, const std::vector<Mesh>& allMeshes, const Frustum& frustum);
DrawLists buildDrawCommands(
	const std::vector<uint32_t>& globalVisibleIndices,
	const std::vector<ObjectData>& objectData,
	const std::vector<Mesh>& allMeshes,
	const std::vector<Submesh>& allSubmeshes,
	const std::vector<Material>& allMaterials);

void generateDebugGeometry(std::vector<DebugVertex>& debugVertices,
	const std::vector<uint32_t>& globalVisibleIndices,
	const std::vector<ObjectData>& objectData,
	const std::vector<Mesh>& allMeshes,
	const std::vector<Submesh>& allSubmeshes,
	bool showMeshAABB, bool showSubmeshAABB);

std::vector<DebugVertex> generateAABBLines(const AABB& aabb, const glm::vec4& color);

void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image);

int main()
{
	// Initialize GLFW & SoLoud
	GLFWwindow* window = createWindow(appState);
	gSoLoud.init();
	gWave.load("../Audio/shadowing.wav");

	uint32_t numCascades = 1;

	VulkanContext context(window);
	Swapchain swapchain(window, context);
	Commands commands(context, MAX_FRAMES_IN_FLIGHT);

	// Create GPU Image resources
	GPUImage image(context, commands);
	image.createDepthImage(swapchain.getExtent().width, swapchain.getExtent().height);
	image.createMSAAColorImage(swapchain.getExtent().width, swapchain.getExtent().height, swapchain.getFormat());

	for (uint32_t i = 0; i < numCascades; ++i)
	{
		image.createShadowMap(swapchain.getExtent().width, swapchain.getExtent().height);
	}

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
	uint32_t sponza = loadModel("../Models/SponzaSeparated/sponzaAABB.obj", image);
	uint32_t alphaTestedGrass = loadModel("../Models/Grass/untitled.obj", image);
	uint32_t glassWindow = loadModel("../Models/GlassWindow/glassWindow.obj", image);
	uint32_t cube = loadModel("../Models/Cube/cube.obj", image);
	uint32_t brickWall = loadModel("../Models/BrickWall/BrickWall.obj", image);

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
	Pipeline debugPipeline(context, swapchain, descriptors, sizeof(DebugPushConstants), "../Shaders/debug_vert.spv", "../Shaders/debug_frag.spv", image.getDepthFormat(), PipelineType::DebugAABB);
	Pipeline shadowPipeline(context, swapchain, descriptors, sizeof(ShadowPushConstants), "../Shaders/shadow_vert.spv", "", image.getDepthFormat(), PipelineType::ShadowMap);

	// Setup syncronization and UI
	Sync sync(context, swapchain, MAX_FRAMES_IN_FLIGHT);
	ImGuiOverlay imgui;
	imgui.init(window, context, descriptors, swapchain.getFormat(), swapchain.getImageCount(), image.getMSAASamples());

	VkDescriptorSet shadowMapImGuiDescriptor = imgui.createImGuiTextureDescriptor(
		image.getShadowMaps()[0].view,
		image.getShadowSampler()
	);

	//Debug labels
	VkDebugUtilsLabelEXT opaquePassLabel = makeLabel("Opaque Pass", 0.0f, 1.0f, 0.0f);
	VkDebugUtilsLabelEXT skyboxPassLabel = makeLabel("Skybox Pass", 0.3f, 0.7f, 1.0f);
	VkDebugUtilsLabelEXT transparentPassLabel = makeLabel("Transparent Pass", 1.0f, 0.5f, 0.0f);
	VkDebugUtilsLabelEXT imguiPassLabel = makeLabel("ImGui Pass", 1.0f, 0.0f, 1.0f);
	VkDebugUtilsLabelEXT debugPassLabel = makeLabel("Debug Wireframe Pass", 1.0f, 1.0f, 0.0f);

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

	Frustum frustum;
	Frustum frozenFrustum;

	// Begin background music
	//gSoLoud.play(gWave, 0.3f, 0.0f, 0.0);

	double lastTime{};
	while (!glfwWindowShouldClose(window))
	{
		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - lastTime);
		lastTime = currentTime;

		// Input & Simulation
		glfwPollEvents();
		processInput(window, deltaTime);
		imgui.newFrame();
		imgui.drawUI();
		updateLighting(lights, deltaTime);
		updateObjects(objectData, lights, deltaTime);

		// Culling & Draw preperation
		pc.view = camera.GetViewMatrix();
		pc.proj = glm::perspective(glm::radians(camera.Zoom),
			(float)appState.windowWidth / (float)appState.windowHeight,
			0.1f, 50.0f);
		pc.proj[1][1] *= -1; // Flip Y for Vulkan

		glm::mat4 viewProj = pc.proj * pc.view;
		frustum.update(viewProj);

		if (imgui.freezeFrustum && !appState.wasFreezeFrustumEnabled)
		{
			frozenFrustum.update(viewProj);
		}
		appState.wasFreezeFrustumEnabled = imgui.freezeFrustum;

		// Choose the frustum to use for culling and perform culling, then build draw lists based on visibility
		const Frustum& cullingFrustum = imgui.freezeFrustum ? frozenFrustum : frustum;
		std::vector<uint32_t> globalVisibleIndices = performFrustumCulling(objectData, allMeshes, cullingFrustum);
		DrawLists drawLists = buildDrawCommands(globalVisibleIndices, objectData, allMeshes, allSubmeshes, allMaterials);

		// Wait for previous frame to finish
		vkWaitForFences(context.getDevice(), 1, sync.getInFlightFencePtr(currentFrame), VK_TRUE, UINT64_MAX);

		// Update GPU resources
		buffer.updateObjectBuffer(objectData.data(), objectData.size() * sizeof(ObjectData), currentFrame);
		buffer.updateLightingBuffer(&lights, sizeof(LightingData), currentFrame);
		if (!globalVisibleIndices.empty())
		{
			buffer.updateVisibleIndexBuffer(globalVisibleIndices.data(), globalVisibleIndices.size() * sizeof(uint32_t), currentFrame);
		}

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

		// Record commands
		VkCommandBuffer cmd = commands.getCommandBuffer(currentFrame);
		vkBeginCommandBuffer(cmd, &beginInfo);

		// Shadowmap render pass
		// 1. Calculate light view-projection matrix
		glm::vec3 lightDir = glm::normalize(glm::vec3(lights.dirLight.direction));
		glm::vec3 lightPos = -lightDir * 20.0f;

		// create light view matrix (looking towards scene center)
		glm::mat4 lightView = glm::lookAt(
			lightPos,
			glm::vec3(0.0f), // scene center
			glm::vec3(0.0f, 1.0f, 0.0f) // up vector
		);

		// Orthographic projection for directional light
		float orthoSize = 50.0f;
		glm::mat4 lightProj = glm::ortho(
			-orthoSize, orthoSize,
			-orthoSize, orthoSize,
			0.1f, 100.0f // near / far plaes
		);
		lightProj[1][1] *= -1; // y-flip for Vulkan

		shadowPC.lightViewProj = lightProj * lightView;

		// 2. Transition shadowmap to attachment optimal
		VkImageMemoryBarrier2 shadowBarrier{};
		shadowBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		shadowBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		shadowBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		shadowBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
		shadowBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		shadowBarrier.image = image.getShadowMaps()[0].image; // first cascade
		shadowBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		shadowBarrier.subresourceRange.baseMipLevel = 0;
		shadowBarrier.subresourceRange.levelCount = 1;
		shadowBarrier.subresourceRange.baseArrayLayer = 0;
		shadowBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo shadowDepInfo{};
		shadowDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		shadowDepInfo.imageMemoryBarrierCount = 1;
		shadowDepInfo.pImageMemoryBarriers = &shadowBarrier;

		vkCmdPipelineBarrier2(cmd, &shadowDepInfo);

		// 3. Shadow Pass rendering info
		VkRenderingAttachmentInfo shadowDepthAttachment{};
		shadowDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		shadowDepthAttachment.imageView = image.getShadowMaps()[0].view;
		shadowDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		shadowDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		shadowDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		shadowDepthAttachment.clearValue.depthStencil = { 1.0f, 0 };

		VkRenderingInfo shadowRenderingInfo{};
		shadowRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		shadowRenderingInfo.renderArea.offset = { 0, 0 };
		shadowRenderingInfo.renderArea.extent = image.getShadowMaps()[0].extent;
		shadowRenderingInfo.layerCount = 1;
		shadowRenderingInfo.colorAttachmentCount = 0; // depth only
		shadowRenderingInfo.pDepthAttachment = &shadowDepthAttachment;

		// 4. Render into shadow map
		vkCmdBeginRendering(cmd, &shadowRenderingInfo);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.getPipeline());

		VkViewport shadowViewport{};
		shadowViewport.x = 0.0f;
		shadowViewport.y = 0.0f;
		shadowViewport.width = (float)image.getShadowMaps()[0].extent.width;
		shadowViewport.height = (float)image.getShadowMaps()[0].extent.height;
		shadowViewport.minDepth = 0.0f;
		shadowViewport.maxDepth = 1.0f;

		VkRect2D shadowScissor{};
		shadowScissor.offset = { 0, 0 };
		shadowScissor.extent = image.getShadowMaps()[0].extent;

		// set all dynamic state
		shadowPipeline.setViewport(cmd, shadowViewport);
		shadowPipeline.setScissor(cmd, shadowScissor);
		shadowPipeline.setCullMode(cmd, VK_CULL_MODE_BACK_BIT);
		shadowPipeline.setDepthTest(cmd, VK_TRUE);
		shadowPipeline.setPolygonMode(cmd, VK_POLYGON_MODE_FILL);

		vkCmdPushConstants(cmd, shadowPipeline.getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &shadowPC);

		// Bind vertex/index buffers
		VkBuffer vertexBuffers[] = { buffer.getVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		// Draw opaque objects only (no transparent/alpha-tested for shadows)
		for (uint32_t matIdx = 0; matIdx < drawLists.opaque.size(); ++matIdx)
		{
			const std::vector<DrawCommand>& drawCmds = drawLists.opaque[matIdx];
			for (const DrawCommand& drawCmd: drawCmds)
			{
				vkCmdDrawIndexed(cmd, drawCmd.indexCount, drawCmd.instanceCount, drawCmd.firstIndex, drawCmd.vertexOffset, drawCmd.firstInstance);
			}
		}
		vkCmdEndRendering(cmd);

		// 5. Transition shadow map to shader read
		shadowBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		shadowBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		shadowBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		shadowBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		shadowBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier2(cmd, &shadowDepInfo);

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

		vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		// Calculate dynamic offset for current frame
		std::array<uint32_t, 3> dynamicOffsets = {
			static_cast<uint32_t>(currentFrame * buffer.getAlignedObjectSize()),
			static_cast<uint32_t>(currentFrame * buffer.getAlignedLightingSize()),
			static_cast<uint32_t>(currentFrame * buffer.getAlignedVisibleIndexBufferSize())
		};

		vkCmdBindDescriptorSets(cmd, 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			scenePipeline.getLayout(), 
			0, 1, &set, 
			static_cast<uint32_t>(dynamicOffsets.size()),
			dynamicOffsets.data());

		pc.cameraPos = camera.Position;
		pc.enableDirectionalLight = imgui.enableDirectionalLight ? 1 : 0;
		pc.enablePointLights = imgui.enablePointLights ? 1 : 0;
		pc.enableNormalMaps = imgui.enableNormalMaps ? 1 : 0;

		// Loop over meshes
		for (uint32_t matIdx = 0; matIdx < drawLists.opaque.size(); ++matIdx)
		{
			const auto& drawCmds = drawLists.opaque[matIdx];
			if (drawCmds.empty())
			{
				continue;
			}
			
			// Set cull mode and upload push constants for this material
			const Material& material = drawCmds[0].material;

			VkCullModeFlagBits cullMode = (material.twosided == 1) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
			scenePipeline.setCullMode(cmd, cullMode);

			pc.enableAlphaTest = (material.alphatest == 1) ? 1 : 0;
			pc.diffuseTextureIndex = static_cast<int>(material.albedoTexture);
			pc.normalTextureIndex = static_cast<int>(material.normalTexture);
			pc.reflectionStrength = material.reflectionStrength;

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
			uint32_t objIndex;				// Global object index
			uint32_t materialIndex;			// Material ID for looking up draw command
			float distanceToCamera;			// For front-to-back sorting
		};

		// Build reverse lookup: global object index -> compact visible array index
		// This is needed because the shader uses gl_InstanceIndex to index into
		// the compact visibleIndices buffer, but we're sorting by global object index
		std::unordered_map<uint32_t, uint32_t> objectToVisibleIndex;
		for (uint32_t i = 0; i < globalVisibleIndices.size(); ++i)
		{
			objectToVisibleIndex[globalVisibleIndices[i]] = i;
		}

		// Collect all transparent objects for sorting
		std::vector<TransparentInstance> transparentObjects;

		for (uint32_t matIdx = 0; matIdx < drawLists.transparent.size(); ++matIdx)
		{
			auto& drawCommands = drawLists.transparent[matIdx];
			for (auto& cmd: drawCommands)
			{
				// Each DrawCommand has a list of visible global object indices
                for (uint32_t i = 0; i < cmd.instanceCount; ++i)
                {
                    uint32_t objIndex = cmd.objectIndices[i];
					glm::vec3 objPos = glm::vec3(objectData[objIndex].model[3]);

                    float dist = glm::length(camera.Position - objPos);
                    transparentObjects.push_back({ objIndex, matIdx, dist });
                }
			}
		}

		// Sort back to front for proper alpha blending
		std::sort(transparentObjects.begin(), transparentObjects.end(),
			[](const TransparentInstance& a, const TransparentInstance& b) {
				return a.distanceToCamera > b.distanceToCamera;
			});

		// Draw transparent objects individually
		for (const auto& inst: transparentObjects)
		{
			const auto& drawCmd = drawLists.transparent[inst.materialIndex][0];
			const auto& mat = drawCmd.material;

			pc.enableAlphaTest = mat.alphatest;
			pc.diffuseTextureIndex = static_cast<int>(mat.albedoTexture);
			pc.normalTextureIndex = static_cast<int>(mat.normalTexture);
			pc.reflectionStrength = mat.reflectionStrength;

			vkCmdPushConstants(cmd, transparentPipeline.getLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(pc), &pc);

			// Covert global object index to compact visible index for shader lookup
			uint32_t visibleIndex = objectToVisibleIndex[inst.objIndex];

			// Draw back faces first, then front faces for correct transparency
			transparentPipeline.setCullMode(cmd, VK_CULL_MODE_FRONT_BIT);
			vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, visibleIndex);

			transparentPipeline.setCullMode(cmd, VK_CULL_MODE_BACK_BIT);
			vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.firstIndex, drawCmd.vertexOffset, visibleIndex);
		}
		vkCmdEndDebugUtilsLabelEXT(cmd);

		// 4. Debug wireframe pass
		if (imgui.showMeshAABB || imgui.showSubmeshAABB)
		{
			std::vector<DebugVertex> debugVertices;
			generateDebugGeometry(debugVertices, globalVisibleIndices, objectData, allMeshes, allSubmeshes,
				imgui.showMeshAABB, imgui.showSubmeshAABB);

			if (!debugVertices.empty())
			{
				// Safely wait for GPU before uploading data. Inefficient but just for debugging.
				vkDeviceWaitIdle(context.getDevice());
				buffer.createOrResizeDebugVertexBuffer(debugVertices.size());
				memcpy(buffer.getDebugBufferMapped(), debugVertices.data(), debugVertices.size() * sizeof(DebugVertex));

				vkCmdBeginDebugUtilsLabelEXT(cmd, &debugPassLabel);
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline.getPipeline());
				debugPipeline.setViewport(cmd, viewport);
				debugPipeline.setScissor(cmd, scissor);

				VkBuffer debugVertexBuffers[] = { buffer.getDebugVertexBuffer() };
				VkDeviceSize debugOffsets[] = { 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, debugVertexBuffers, debugOffsets);

				debugPC.view = pc.view;
				debugPC.proj = pc.proj;

				vkCmdPushConstants(cmd, debugPipeline.getLayout(),
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(debugPC), &debugPC);

				vkCmdDraw(cmd, static_cast<uint32_t>(debugVertices.size()), 1, 0, 0);
				vkCmdEndDebugUtilsLabelEXT(cmd);
			}
		}
		vkCmdEndRendering(cmd);

		// 5. UI pass
		vkCmdBeginDebugUtilsLabelEXT(cmd, &imguiPassLabel);
		imgui.drawShadowMapVisualization(shadowMapImGuiDescriptor);
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
	AABB bounds;

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	// record where new materials will start in global array
	uint32_t baseMaterialIndex = static_cast<uint32_t>(allMaterials.size());

	// iterate shapes
	for (const auto& shape : shapes) {
		Submesh sub{};
		sub.indexOffset = static_cast<uint32_t>(allIndices.size());
		AABB subBounds; 

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

		// Verify that all faces are triangles before processing
		for (unsigned char fv : shape.mesh.num_face_vertices) {
			if (fv != 3) {
				throw std::runtime_error("Non-triangular face found in OBJ file. Please triangulate before loading.");
			}
		}

		for (const tinyobj::index_t& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			// Expand bounds
			bounds.expand(vertex.pos);
			subBounds.expand(vertex.pos);

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

			// MikkTSpace will populate later
			vertex.tangent = glm::vec4(0.0f);

			if (!uniqueVertices.contains(vertex)) {
				uniqueVertices[vertex] = static_cast<uint32_t>(allVertices.size() - mesh.vertexOffset);
				allVertices.push_back(vertex);
			}

			allIndices.push_back(uniqueVertices[vertex]);
		}

		sub.indexCount = static_cast<uint32_t>(allIndices.size()) - sub.indexOffset;
		sub.bounds = subBounds;
		allSubmeshes.push_back(sub);
	}

	mesh.vertexCount = static_cast<uint32_t>(allVertices.size() - mesh.vertexOffset);
	mesh.submeshCount = static_cast<uint32_t>(allSubmeshes.size()) - mesh.submeshOffset;
	mesh.bounds = bounds;

	uint32_t meshIndex = static_cast<uint32_t>(allMeshes.size());
	allMeshes.push_back(mesh);

	// Calculate tangents for the loaded mesh's submeshes using MikkTSpace
	for (uint32_t i = 0; i < mesh.submeshCount; ++i)
	{
		const Submesh& sub = allSubmeshes[mesh.submeshOffset + i];

		MikkTSpaceData data;
		data.allVerticesPtr = &allVertices;
		data.allIndicesPtr = &allIndices; // Using a single index buffer

		data.vertexOffset = mesh.vertexOffset; // Mesh's start vertex in allVertices
		data.indexOffset = sub.indexOffset; // Submesh's start index in allIndices
		data.indexCount = sub.indexCount; // Submesh's index count

		// Use static helper function to run calculation
		TangentGenerator::CalculateTangents(data);
	}

	// Load material data
	for (const auto& mtl : materials)
	{
		Material mat{};

		if (!mtl.diffuse_texname.empty())
		{
			fs::path texPath = objDir / mtl.diffuse_texname;
			mat.albedoTexture = imageClass.loadTexture(texPath.string(), true); // Pass true for SRGB
		}
		else
		{
			mat.albedoTexture = -1;
		}

		if (!mtl.bump_texname.empty())
		{
			fs::path texPath = objDir / mtl.bump_texname;
			mat.normalTexture = imageClass.loadTexture(texPath.string(), false); // Pass false for UNORM/data
		}
		else
		{
			mat.normalTexture = -1;
		}

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
	// Define spacing between cubes
	float spacing = 3.0f;
	uint32_t gridSize = 10;
	uint32_t meshIndex = static_cast<uint32_t>(MeshType::Cube);

	// Iterate rows
	for (uint32_t x = 0; x < gridSize; ++x)
	{
		// Iterate cols
		for (uint32_t z = 0; z < gridSize; ++z)
		{
			// Calculate the current cube's position
			glm::vec3 pos = {
				(float)x * spacing,
				0.0f,
				(float)z * spacing
			};

			glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
			objectData.push_back({ model, meshIndex });
		}
	}

	// 3D cube grid
	spacing = 1.0f; // Very dense spacing
	uint32_t sizeX = 1;
	uint32_t sizeY = 1;
	uint32_t sizeZ = 1;
	meshIndex = static_cast<uint32_t>(MeshType::Cube);

	// Initial offset to place the grid away from the origin
	glm::vec3 baseOffset = { 0.0f, 10.0f, 0.0f };

	for (uint32_t x = 0; x < sizeX; ++x)
	{
		for (uint32_t y = 0; y < sizeY; ++y)
		{
			for (uint32_t z = 0; z < sizeZ; ++z)
			{
				glm::vec3 pos = {
					baseOffset.x + (float)x * spacing,
					baseOffset.y + (float)y * spacing,
					baseOffset.z + (float)z * spacing
				};

				glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
				model = glm::scale(model, glm::vec3(0.4f));

				objectData.push_back({ model, meshIndex });
			}
		}
	}

	// Add windows
	spacing = 2.0f;
	gridSize = 4;
	meshIndex = static_cast<uint32_t>(MeshType::GlassWindow);

	// Iterate rows
	for (uint32_t x = 0; x < gridSize; ++x)
	{
		// Iterate cols
		for (uint32_t z = 0; z < gridSize; ++z)
		{
			// Calculate the current cube's position
			glm::vec3 pos = {
				(float)x * spacing,
				5.0f,
				(float)z * spacing
			};

			glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
			objectData.push_back({ model, meshIndex });
		}
	}

	// Add a sponza
	glm::vec3 pos = { 50.0f, -10.0f, 5.0f };
	glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Sponza);
	objectData.push_back({ model, meshIndex });

	// Add brick wall
	pos = { -10.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::BrickWall);
	objectData.push_back({ model, meshIndex });

	buffer.createObjectBuffer(objectData.size());
	buffer.createVisibleIndexBuffer(objectData.size());
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

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime)
{
	// Static time variable for oscillation
	static float t = 0.0f;
	t += deltaTime;

	// Pure rotation matrix for this frame
	glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(60.0f * deltaTime), glm::vec3(0.0f, 1.0f, 0.0f));
	// Calculate a pulsing scale factor between 1.0 and 2.0
	float scaleFactor = 1.5f + 0.5f * glm::sin(t * 2.0f); // S = [1.0, 2.0]

	// Reference object and extract world position
	glm::mat4& cubeModel = objectData[0].model;
	glm::vec3 cubePos = glm::vec3(cubeModel[3]);
	
	// Oscillate the cube up and down
	float initialY = 0.0f;
	cubePos.y = initialY + 3.0f * glm::sin(t * 0.5f);

	// Uniform Scale Extraction: Calculate the length of the first basis vector (X-axis)
	float oldScale = glm::length(glm::vec3(cubeModel[0]));

	// Normalize the rotation/scale part (removes scale component)
	cubeModel[0] = glm::vec4(glm::normalize(glm::vec3(cubeModel[0])), 0.0f);
	cubeModel[1] = glm::vec4(glm::normalize(glm::vec3(cubeModel[1])), 0.0f);
	cubeModel[2] = glm::vec4(glm::normalize(glm::vec3(cubeModel[2])), 0.0f);
	cubeModel[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Remove translation

	// Apply pure rotation (on the left for local spin)
	cubeModel = rotationMatrix * cubeModel;

	// Restore new uniform scale
	cubeModel[0] *= scaleFactor;
	cubeModel[1] *= scaleFactor;
	cubeModel[2] *= scaleFactor;

	// Restore translation
	cubeModel[3] = glm::vec4(cubePos, 1.0f);
}

std::vector<uint32_t> performFrustumCulling(std::vector<ObjectData>& objectData, const std::vector<Mesh>& allMeshes, const Frustum& frustum)
{
	// Visibility flag per-thread
	std::vector<uint8_t> visibility(objectData.size(), 0);

	// Parallel visibility test using ranges, as Frustum Culling is "embarrassingly parallel"
	auto indices = std::views::iota(0u, static_cast<uint32_t>(objectData.size()));
	std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
		[&](uint32_t i)
		{
			// Transform mesh AABB to world space and check visibility against frustum
			const auto& mesh = allMeshes[objectData[i].meshIndex];
			AABB worldBounds = mesh.bounds.transform(objectData[i].model);

			// Sphere test is slightly faster, often used for first pass
			//bool visible = frustum.isBoxVisible(worldBounds.min, worldBounds.max);
			bool visible = frustum.isSphereVisible(worldBounds.center(), worldBounds.radius());

			objectData[i].isVisible = visible ? 1 : 0;
			visibility[i] = visible ? 1 : 0;
		});

	std::vector<uint32_t> globalVisibleIndices;
	globalVisibleIndices.reserve(objectData.size());

	for (uint32_t i = 0; i < visibility.size(); ++i)
	{
		if (visibility[i])
		{
			globalVisibleIndices.push_back(i);
		}
	}

	return globalVisibleIndices;
}

DrawLists buildDrawCommands(const std::vector<uint32_t>& globalVisibleIndices, const std::vector<ObjectData>& objectData, const std::vector<Mesh>& allMeshes, const std::vector<Submesh>& allSubmeshes, const std::vector<Material>& allMaterials)
{
	DrawLists result;
	result.opaque.resize(allMaterials.size());
	result.transparent.resize(allMaterials.size());

	// Group visible objects by mesh (ONE TIME)
	std::unordered_map<uint32_t, std::vector<uint32_t>> visibleByMesh;
	for (uint32_t objIdx : globalVisibleIndices)
	{
		uint32_t meshIdx = objectData[objIdx].meshIndex;
		visibleByMesh[meshIdx].push_back(objIdx);
	}

	// Build sorted list of visible mesh indices for correct offset ordering
	std::vector<uint32_t> sortedVisibleMeshIndices;
	std::unordered_set<uint32_t> seenMeshes;

	for (uint32_t objectIndex: globalVisibleIndices)
	{
		uint32_t meshIndex = objectData[objectIndex].meshIndex;
		if (seenMeshes.insert(meshIndex).second)
		{
			sortedVisibleMeshIndices.push_back(meshIndex);
		}
	}

	// Tracks where each mesh's instances start in the globalVisibleIndices buffer
	uint32_t globalInstanceOffset = 0;

	// Process each mesh in the order they first appear in objectData
	for (uint32_t meshIndex : sortedVisibleMeshIndices)
	{
		const auto& visibleIndices = visibleByMesh[meshIndex];
		const Mesh& mesh = allMeshes[meshIndex];

		uint32_t visibleInstanceCount = static_cast<uint32_t>(visibleIndices.size());

		// Process each submesh(different geometry parts with potentially different materials)
		for (uint32_t submeshIdx = 0; submeshIdx < mesh.submeshCount; ++submeshIdx)
		{
			const Submesh& submesh = allSubmeshes[mesh.submeshOffset + submeshIdx];
			const Material& material = allMaterials[submesh.materialIndex];

			// Construct the draw command
			DrawCommand cmd{};
			cmd.indexCount = submesh.indexCount;
			cmd.instanceCount = visibleInstanceCount;
			cmd.firstIndex = submesh.indexOffset;
			cmd.vertexOffset = static_cast<uint32_t>(mesh.vertexOffset);
			cmd.material = material;

			// Split opaque vs transparent
			if (material.alphablending == 1)
			{
				cmd.firstInstance = 0; // we will draw transparent objects individually
				cmd.objectIndices = visibleIndices;
				result.transparent[submesh.materialIndex].push_back(cmd);
			}
			else
			{
				cmd.firstInstance = globalInstanceOffset; // Same offset for ALL submeshes of this mesh

				// Optimization: Merge with previous draw if possible (reduces draw call count)
				auto& opaqueList = result.opaque[submesh.materialIndex];
				if (!opaqueList.empty())
				{
					DrawCommand& lastCmd = opaqueList.back();

					// Can merge if: same mesh with contiguous index range
					bool canMerge = (lastCmd.vertexOffset == cmd.vertexOffset &&
						lastCmd.instanceCount == cmd.instanceCount &&
						lastCmd.firstInstance == cmd.firstInstance &&
						lastCmd.firstIndex + lastCmd.indexCount == cmd.firstIndex);

					if (canMerge)
					{
						// Just extend the previous command's index range
						lastCmd.indexCount += cmd.indexCount;
						continue; // Skip adding a new command
					}
				}

				result.opaque[submesh.materialIndex].push_back(cmd);
			}
		}

		// Advance the global offset by the number of visible instances of this mesh
		// This ensures the next mesh's instanaces start at the correct position in the buffer
		globalInstanceOffset += visibleInstanceCount;
	}
	return result;
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

void generateDebugGeometry(std::vector<DebugVertex>& debugVertices,
	const std::vector<uint32_t>& globalVisibleIndices,
	const std::vector<ObjectData>& objectData,
	const std::vector<Mesh>& allMeshes,
	const std::vector<Submesh>& allSubmeshes,
	bool showMeshAABB, bool showSubmeshAABB)
{
	debugVertices.clear();

	for (uint32_t objectIndex: globalVisibleIndices)
	{
		const auto& obj = objectData[objectIndex];
		const Mesh& mesh = allMeshes[obj.meshIndex];

		// Draw mesh-level AABB in red
		if (showMeshAABB)
		{
			AABB worldBounds = mesh.bounds.transform(obj.model);
			auto lines = generateAABBLines(worldBounds, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			debugVertices.insert(debugVertices.end(), lines.begin(), lines.end());
		}

		// Draw submesh-level AABBs in green
		if (showSubmeshAABB)
		{
			for (uint32_t i = 0; i < mesh.submeshCount; ++i)
			{
				const Submesh& submesh = allSubmeshes[mesh.submeshOffset + i];
				AABB worldSubBounds = submesh.bounds.transform(obj.model);
				auto lines = generateAABBLines(worldSubBounds, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				debugVertices.insert(debugVertices.end(), lines.begin(), lines.end());
			}
		}
	}
}

std::vector<DebugVertex> generateAABBLines(const AABB& aabb, const glm::vec4& color)
{
	std::vector<DebugVertex> lines;
	lines.reserve(24); // 12 edges * 2 vertices per edge

	glm::vec3 corners[8] = {
		{aabb.min.x, aabb.min.y, aabb.min.z}, // 0
		{aabb.max.x, aabb.min.y, aabb.min.z}, // 1
		{aabb.max.x, aabb.max.y, aabb.min.z}, // 2
		{aabb.min.x, aabb.max.y, aabb.min.z}, // 3
		{aabb.min.x, aabb.min.y, aabb.max.z}, // 4
		{aabb.max.x, aabb.min.y, aabb.max.z}, // 5
		{aabb.max.x, aabb.max.y, aabb.max.z}, // 6
		{aabb.min.x, aabb.max.y, aabb.max.z}  // 7
	};

	// Bottom face (z = min)
	lines.push_back({ corners[0], color }); lines.push_back({ corners[1], color });
	lines.push_back({ corners[1], color }); lines.push_back({ corners[2], color });
	lines.push_back({ corners[2], color }); lines.push_back({ corners[3], color });
	lines.push_back({ corners[3], color }); lines.push_back({ corners[0], color });

	// Top face (z = max)
	lines.push_back({ corners[4], color }); lines.push_back({ corners[5], color });
	lines.push_back({ corners[5], color }); lines.push_back({ corners[6], color });
	lines.push_back({ corners[6], color }); lines.push_back({ corners[7], color });
	lines.push_back({ corners[7], color }); lines.push_back({ corners[4], color });

	// Vertical edges
	lines.push_back({ corners[0], color }); lines.push_back({ corners[4], color });
	lines.push_back({ corners[1], color }); lines.push_back({ corners[5], color });
	lines.push_back({ corners[2], color }); lines.push_back({ corners[6], color });
	lines.push_back({ corners[3], color }); lines.push_back({ corners[7], color });

	return lines;
}

// Use AABBs to do 3D collision checks
AABB get_world_aabb(const ObjectData& obj, const Mesh& mesh)
{
	return mesh.bounds.transform(obj.model);
}

bool check_collision(const ObjectData& objA, const ObjectData& objB, const std::vector<Mesh>& allMeshes)
{
	const Mesh& meshA = allMeshes[objA.meshIndex];
	const Mesh& meshB = allMeshes[objB.meshIndex];

	AABB worldA = get_world_aabb(objA, meshA);
	AABB worldB = get_world_aabb(objB, meshB);

	return worldA.overlaps(worldB);
}
