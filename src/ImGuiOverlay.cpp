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
	info.DescriptorPool = descriptors.getDescriptorPool();
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

	ImGui::Separator();

	ImGui::Checkbox("Enable Depth Test", &enableDepthTest);
	ImGui::Checkbox("Enable Wireframe", &enableWireframe);

	ImGui::Separator();
	ImGui::Text("Lighting");
	ImGui::Checkbox("Enable Directional Light", &enableDirectionalLight);
	ImGui::Checkbox("Enable Point Lights", &enablePointLights);

	ImGui::Separator();
	ImGui::Checkbox("Show Metrics", &showMetrics);
	if (showMetrics)
	{
		ImGui::ShowMetricsWindow(&showMetrics);
	}

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
