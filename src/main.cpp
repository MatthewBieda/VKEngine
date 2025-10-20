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
	uint32_t padding1 = 0;
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
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window, float deltaTime);

uint32_t loadModel(const std::string& modelPath, GPUImage& imageClass);

enum class MeshType
{
	LightCaster,
	Sponza,
	AlphaTestedGrass,
	GlassWindow
};

void setupSceneObjects(GPUBuffer& buffer, std::vector<ObjectData>& objectData);
void setupLighting(GPUBuffer& buffer, LightingData& lights);
void updateLighting(LightingData& lights, float deltaTime);
void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime);

void recreateSwapchainResources(VulkanContext& context, Swapchain& swapchain, GPUImage& image);

int main()
{
	// Initialize Vulkan core
	GLFWwindow* window = createWindow(appState);
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
	uint32_t alphaTestedGrass = loadModel("../Models/Grass/untitled.obj", image);
	uint32_t glassWindow = loadModel("../Models/GlassWindow/glassWindow.obj", image);

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
		imgui.drawUI();

		updateLighting(lights, deltaTime);
		updateObjects(objectData, lights, deltaTime);

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

	// Light casters
	for (int i = 0; i < lights.numPointLights; ++i)
	{
		meshIndex = static_cast<uint32_t>(MeshType::LightCaster);
		objectData.push_back({ model, meshIndex });
	}

	// Sponza
	pos = { 0.0f, 0.0f, 0.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Sponza);
	objectData.push_back({ model, meshIndex });

	// Sponza 2nd instance
	pos = { 0.0f, 0.0f, 30.0f };
	model = glm::translate(glm::mat4(1.0f), pos);
	meshIndex = static_cast<uint32_t>(MeshType::Sponza);
	objectData.push_back({ model, meshIndex });

	// Windows
	for (float i = 0.0f; i < 3.0f; ++i)
	{
		pos = { i * 2.0f, 5.0f, -0.5f };
		model = glm::translate(glm::mat4(1.0f), pos);
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		meshIndex = static_cast<uint32_t>(MeshType::GlassWindow);
		objectData.push_back({ model, meshIndex });
	}

	buffer.createObjectBuffer(objectData.size());
	buffer.updateObjectBuffer(objectData.data(), objectData.size() * sizeof(ObjectData), currentFrame);
}

void setupLighting(GPUBuffer& buffer, LightingData& lights)
{
	lights.dirLight.direction = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);
	lights.dirLight.color = glm::vec4(1.0f);

	lights.numPointLights = 100;

	for (int i = 0; i < 100; ++i)
	{
		lights.pointLights[i].position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		// Cycle through colors
		glm::vec3 colors[] = {
			{1.0f, 0.0f, 0.0f},   // red
			{0.0f, 1.0f, 0.0f},   // green
			{0.0f, 0.0f, 1.0f},   // blue
			{1.0f, 1.0f, 0.0f},   // yellow
			{1.0f, 0.0f, 1.0f}    // magenta
		};
		lights.pointLights[i].color = glm::vec4(colors[i % 5], 1.0f);
		lights.pointLights[i].radius = 5.0f;
	}

	buffer.createLightingBuffer(sizeof(LightingData));
	buffer.updateLightingBuffer(&lights, sizeof(LightingData), currentFrame);
}

void updateLighting(LightingData& lights, float deltaTime)
{
	static float t = 0.0f;
	t += deltaTime;

	for (int i = 0; i < lights.numPointLights; ++i)
	{
		int ring = i / 20;  // ring index
		int posInRing = i % 20;

		// ring properties
		float baseRadius = 2.0f + ring * 1.5f;
		float height = 1.0f + ring * 0.8f;
		float rotationSpeed = 1.0f - (ring * 0.15f);  // Inner rings faster

		float lightPos = (posInRing / 20.0f) * glm::radians(360.0f) + t * rotationSpeed;

		lights.pointLights[i].position = glm::vec4(
			baseRadius * cos(lightPos),
			height,
			baseRadius * sin(lightPos),
			1.0f
		);
	}
}

void updateObjects(std::vector<ObjectData>& objectData, const LightingData& lights, float deltaTime)
{
	static float t = 0.0f;
	t += deltaTime;

	// Sync light casters with point lights position
	for (int i = 0; i < lights.numPointLights; ++i)
	{
		glm::vec3 pos = glm::vec3(lights.pointLights[i].position);
		glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
		model = glm::rotate(model, t * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)); // Spin around y-axis
		objectData[i].model = model;
	}

	// Orbiting windows
	glm::vec3 orbitCenter = glm::vec3(0.0f, 6.0f, -0.5f); // Pivot point for windows
	constexpr float orbitRadius = 4.0f; // Distance from center
	constexpr float orbitSpeed = glm::radians(45.0f); // degrees per second
	constexpr float angularOffset = glm::radians(120.0f); // 3 window evenly spaced

	for (int i = 102; i <= 104; ++i)
	{
		// Compute angular position
		const int windowIndex = i - 102;
		const float baseAngle = windowIndex * angularOffset;
		const float currentAngle = baseAngle + t * orbitSpeed;

		// Orbit position
		glm::vec3 orbitPos = orbitCenter + glm::vec3(
			orbitRadius * cos(currentAngle),
			0.0f,
			orbitRadius * sin(currentAngle)
		);

		// Build model matrix
		glm::mat4 model = glm::translate(glm::mat4(1.0f), orbitPos);

		// Rotate so window faces the center, then apply orientation correction
		model = glm::rotate(model, -currentAngle, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		objectData[i].model = model;
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
