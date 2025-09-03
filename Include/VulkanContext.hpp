#pragma once

#include "volk.h"
#include <GLFW/glfw3.h>

class VulkanContext
{
public:
	VulkanContext(GLFWwindow* window);

	VulkanContext(const VulkanContext&) = delete;
	VulkanContext& operator=(const VulkanContext&) = delete;
	VulkanContext(VulkanContext&&) = delete;
	VulkanContext& operator=(VulkanContext&&) = delete;

	~VulkanContext();

	VkInstance getInstance() const { return m_instance; }
	VkDevice getDevice() const { return m_device; }
	VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
	VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
	int getGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }
	VkSurfaceKHR getSurface() const { return m_surface; }

private:
	void initInstance();
	void initDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();

	VkInstance m_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	VkQueue m_presentQueue = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	GLFWwindow* m_window;
	
	uint32_t m_graphicsQueueFamilyIndex = -1;
	uint32_t m_presentQueueFamilyIndex = -1;

	// Validation layer callback
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
};