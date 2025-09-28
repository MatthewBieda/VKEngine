#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "glm.hpp"

// Forward declarations
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

	bool showMetrics = false;
	glm::vec3 clearColor = { 0.1f, 0.5f, 1.0f };
	inline static bool enableDepthTest = VK_TRUE;
	inline static bool enableWireframe = VK_FALSE;
	inline static bool enableBackfaceCulling = VK_FALSE;

private:
	static void checkVkResult(VkResult err);
	bool m_initialized = false;
};
