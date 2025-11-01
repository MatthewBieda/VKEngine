#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "glm.hpp"

class VulkanContext;
class DescriptorManager;

class ImGuiOverlay
{
public:
	ImGuiOverlay() = default;
	~ImGuiOverlay();

	// Initalize ImGui with Vulkan context
	void init(GLFWwindow* window, VulkanContext& context, DescriptorManager& descriptors, VkFormat swapchainFormat, uint32_t imageCount, VkSampleCountFlagBits msaaSamples);

	// Begin new frame
	void newFrame();

	// Record ImGui commands to command buffer
	void recordCommands(VkCommandBuffer commandBuffer);

	void render();

	void drawUI();

	inline static bool showMetrics = VK_TRUE;
	inline static bool enableDepthTest = VK_TRUE;
	inline static bool enableWireframe = VK_FALSE;
	inline static bool enableDirectionalLight = VK_TRUE;
	inline static bool enablePointLights = VK_TRUE;
	inline static bool freezeFrustum = VK_FALSE;
	inline static bool showMeshAABB = VK_FALSE;
	inline static bool showSubmeshAABB = VK_FALSE;
	inline static bool enableNormalMaps = VK_TRUE;

private:
	static void checkVkResult(VkResult err);
	bool m_initialized = false;
};
