#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// Forward declarations
class VulkanContext;
class DescriptorManager;

class ImGuiOverlay
{
public:
	ImGuiOverlay() = default;
	~ImGuiOverlay();

	// Initaliz ImGui with Vulkan context
	void init(GLFWwindow* window, VulkanContext& context, DescriptorManager& descriptors, VkFormat swapchainFormat, uint32_t imageCount, VkSampleCountFlagBits msaaSamples);

	// Begin new frame
	void newFrame();

	// Record ImGui commands to command buffer
	void recordCommands(VkCommandBuffer commandBuffer);

	void render();

	void drawUI();

private:
	static void checkVkResult(VkResult err);
	bool m_initialized = false;
};