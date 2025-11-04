#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES

#include "ImGuiOverlay.hpp"
#include "VulkanContext.hpp"
#include "DescriptorManager.hpp"

#include <stdexcept>
#include <iostream>

ImGuiOverlay::~ImGuiOverlay()
{
	if (m_initialized)
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}

void ImGuiOverlay::init(GLFWwindow* window, VulkanContext& context, DescriptorManager& descriptors, VkFormat swapchainFormat, uint32_t imageCount, VkSampleCountFlagBits msaaSamples)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	// ImGui - GLFW initialization
	ImGui_ImplGlfw_InitForVulkan(window, true);

	// Load Vulkan functions for ImGui with optimal device/instance function loading
	auto funcLoader = [](const char* funcName, void* userData) -> PFN_vkVoidFunction {
		VulkanContext* ctx = reinterpret_cast<VulkanContext*>(userData);

		// List of functions that are definitely instance-level only
		static const char* instanceOnlyFunctions[] = {
			"vkDestroySurfaceKHR",
			"vkEnumeratePhysicalDevices",
			"vkGetPhysicalDeviceProperties",
			"vkGetPhysicalDeviceMemoryProperties",
			"vkGetPhysicalDeviceQueueFamilyProperties",
			"vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
			"vkGetPhysicalDeviceSurfaceFormatsKHR",
			"vkGetPhysicalDeviceSurfacePresentModesKHR",
			"vkGetPhysicalDeviceSurfaceSupportKHR",
			"vkCreateInstance",
			"vkDestroyInstance",
			"vkEnumerateInstanceExtensionProperties",
			"vkEnumerateInstanceLayerProperties"
		};

		// Check if this is an instance-only function
		for (const char* instanceFunc : instanceOnlyFunctions) {
			if (strcmp(funcName, instanceFunc) == 0) {
				return vkGetInstanceProcAddr(ctx->getInstance(), funcName);
			}
		}

		// For all other functions, try device first, then fall back to instance
		PFN_vkVoidFunction deviceAddr = vkGetDeviceProcAddr(ctx->getDevice(), funcName);
		if (deviceAddr) {
			return deviceAddr;
		}

		return vkGetInstanceProcAddr(ctx->getInstance(), funcName);
		};

	bool funcsLoaded = ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_4, funcLoader, &context);
	if (!funcsLoaded) {
		throw std::runtime_error("Failed to load Vulkan functions for ImGui!");
	}

	// ImGui - Vulkan initialization
	ImGui_ImplVulkan_InitInfo info{};
	info.Instance = context.getInstance();
	info.ApiVersion = VK_API_VERSION_1_4;
	info.PhysicalDevice = context.getPhysicalDevice();
	info.Device = context.getDevice();
	info.QueueFamily = context.getGraphicsQueueFamilyIndex();
	info.Queue = context.getGraphicsQueue();
	info.PipelineCache = VK_NULL_HANDLE;
	info.DescriptorPoolSize = 5; // Use backend pools instead of mine
	info.RenderPass = VK_NULL_HANDLE;
	info.Subpass = 0;
	info.MinImageCount = imageCount;
	info.ImageCount = imageCount;
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.Allocator = nullptr;
	info.UseDynamicRendering = true;
	info.CheckVkResultFn = checkVkResult;

	// Setup pipeline rendering info for dynamic rendering
	VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
	pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingInfo.colorAttachmentCount = 1;
	pipelineRenderingInfo.pColorAttachmentFormats = &swapchainFormat;

	info.PipelineRenderingCreateInfo = pipelineRenderingInfo;

	bool initResult = ImGui_ImplVulkan_Init(&info);
	if (!initResult)
	{
		throw std::runtime_error("Failed to initialize ImGui VUlkan implementation");
	}

	m_initialized = true;
	std::cout << "ImGui initialized successfully with existing Vulkan resources!" << std::endl;
}

void ImGuiOverlay::newFrame()
{
	if (!m_initialized)
	{
		return;
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiOverlay::recordCommands(VkCommandBuffer commandBuffer)
{
	if (!m_initialized)
	{
		return;
	}

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiOverlay::render()
{
	if (!m_initialized)
	{
		return;
	}

	ImGui::Render();
}

void ImGuiOverlay::drawUI()
{
	if (!m_initialized) 
	{
		return;
	}
	ImGuiID id = 0;
	ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// Control panel
	ImGui::Begin("VKEngine Controls");

	if (ImGui::CollapsingHeader("Global Rendering"))
	{
		ImGui::Checkbox("Enable Depth Test", &enableDepthTest);
		ImGui::Checkbox("Enable Wireframe", &enableWireframe);
		ImGui::Checkbox("Enable Normal Maps", &enableNormalMaps);
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::Checkbox("Enable Directional Light", &enableDirectionalLight);
		ImGui::Checkbox("Enable Point Lights", &enablePointLights);
	}

	if (ImGui::CollapsingHeader("Debug"))
	{
		ImGui::Checkbox("Show Mesh AABB (Red)", &showMeshAABB);
		ImGui::Checkbox("Show Submesh AABB (Green)", &showSubmeshAABB);
		ImGui::Checkbox("Freeze Camera Frustum", &freezeFrustum);
	}

	if (ImGui::CollapsingHeader("Render Targets"))
	{
		ImGui::Checkbox("Show Shadow Map", &showShadowMap);
	}

	ImGui::Separator();
	ImGui::Text("Metrics");
	ImGui::Checkbox("Show Metrics", &showMetrics);
	if (showMetrics)
	{
		ImGui::ShowMetricsWindow(&showMetrics);
	}

	ImGui::End();
}

VkDescriptorSet ImGuiOverlay::createImGuiTextureDescriptor(VkImageView imageView, VkSampler sampler)
{
	VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return descriptorSet;
}

void ImGuiOverlay::drawShadowMapVisualization(
	const std::array<VkDescriptorSet, 4>& shadowMapDescriptorSets,
	const std::vector<ShadowCascades::CascadeData>& cascades) const
{
	// 1. Check if the window should be drawn
	if (!showShadowMap || !m_initialized)
	{
		return;
	}
	ImGui::Begin("Shadow Maps", &showShadowMap);

	constexpr float SHADOW_MAP_SIZE = 2048.0f;

	// Get available content region width
	float windowWidth = ImGui::GetContentRegionAvail().x;

	// Determine the number of cascades per row
	constexpr int CASCADES_PER_ROW = 2;

	// Calculate the size for each image, accounting for padding/spacing
	// ImGui::GetStyle().ItemSpacing.x is the space added by ImGui::SameLine()
	float padding = ImGui::GetStyle().ItemSpacing.x * (CASCADES_PER_ROW - 1);
	float imageSize = (windowWidth - padding) / CASCADES_PER_ROW;

	// Enforce a minimum size for readability
	imageSize = glm::max(imageSize, 100.0f);

	for (int i = 0; i < ShadowCascades::NUM_CASCADES; ++i)
	{
		// Draw each cascade texture
		ImGui::BeginGroup(); // Group text and image together

		// Use a specific color for the text to match the common debug colors
		ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		if (i == 0) color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f); // Red
		if (i == 1) color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // Green
		if (i == 2) color = ImVec4(0.5f, 0.5f, 1.0f, 1.0f); // Blue
		if (i == 3) color = ImVec4(1.0f, 1.0f, 0.5f, 1.0f); // Yellow

		ImGui::TextColored(color, "Cascade %d", i);

		// Use the constant calculated size for a square image
		ImGui::Image((ImTextureID)shadowMapDescriptorSets[i], ImVec2(imageSize, imageSize));

		// Calculate range and resolution per meter
		float range = cascades[i].farDepth - cascades[i].nearDepth;
		float pixelsPerMeter = SHADOW_MAP_SIZE / range;

		ImGui::Text("Range: %.1fm - %.1fm", cascades[i].nearDepth, cascades[i].farDepth);
		ImGui::Text("Depth: %.1fm", range);
		ImGui::TextColored(color, "~%.0f px/m", pixelsPerMeter);

		ImGui::EndGroup();

		// If not the last cascade and it's an odd index (0, 2, etc.), put the next one on the same line
		if ((i + 1) % CASCADES_PER_ROW != 0 && i < ShadowCascades::NUM_CASCADES - 1)
		{
			ImGui::SameLine();
		}
	}

	// 3. End the window
	ImGui::End();
}

void ImGuiOverlay::checkVkResult(VkResult err)
{
	if (err == VK_SUCCESS) return;

	std::cerr << "[Vulkan] Error: VkResult = " << err << std::endl;
	if (err < 0) 
	{
		throw std::runtime_error("Fatal Vulkan error occurred in ImGui!");
	}
}
