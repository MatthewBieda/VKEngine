#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION

#define VMA_IMPLEMENTATION

#include "vk_mem_alloc.h"

#include "Utils.hpp"
#include "VulkanContext.hpp"

#include <iostream>
#include <vector>

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VulkanContext::VulkanContext(GLFWwindow* window) : m_window(window)
{
	if (volkInitialize() != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to initialize Volk!");
	}

	initInstance();
	volkLoadInstance(m_instance);
	initDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createAllocator();
	volkLoadDevice(m_device);

	nameObject(m_device, m_instance, "VulkanInstance");
	nameObject(m_device, m_device, "Device");
	nameObject(m_device, m_physicalDevice, "PhysicalDevice");
	nameObject(m_device, m_surface, "Surface");
	nameObject(m_device, m_graphicsQueue, "Queue_Graphics");
}

VulkanContext::~VulkanContext()
{
	vmaDestroyAllocator(m_allocator);
	vkDestroyDevice(m_device, nullptr);
	vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::initInstance()
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanApp";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VKEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> extensions = { "VK_EXT_debug_utils", "VK_KHR_surface", "VK_KHR_win32_surface" };

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instanceCreateInfo.ppEnabledLayerNames = layers.data();
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan instance!");
	}
}

void VulkanContext::initDebugMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugCallback;

	if (vkCreateDebugUtilsMessengerEXT(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create debug messenger!");
	}
}

void VulkanContext::createSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

void VulkanContext::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		throw std::runtime_error("No Vulkan devices found!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	int bestScore = -1;
	for (const VkPhysicalDevice& device : devices)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		int score = 0;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		if (score > bestScore)
		{
			bestScore = score;
			m_physicalDevice = device;
		}
	}

	// Query swapchain support
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	bool swapchainSupported = false;
	for (const VkExtensionProperties& extension : availableExtensions)
	{
		if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
		{
			swapchainSupported = true;
			break;
		}
	}

	if (!swapchainSupported)
	{
		throw std::runtime_error("Selected device does not support VK_KHR_swapchain!");
	}

	// Query queue families
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, families.data());

	for (int i = 0; i < families.size(); ++i)
	{
		// Graphics
		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_graphicsQueueFamilyIndex = i;
		}

		// Present
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
		if (presentSupport)
		{
			m_presentQueueFamilyIndex = i;
		}

		// Early exit if both capabilities found in the same family
		if (m_graphicsQueueFamilyIndex != -1 && m_presentQueueFamilyIndex != -1)
		{
			break;
		}
	}

	if (m_graphicsQueueFamilyIndex == -1)
	{
		throw std::runtime_error("No queue family supports graphics!");
	}

	if (m_presentQueueFamilyIndex == -1)
	{
		throw std::runtime_error("Presentation not supported!");

	}

	if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex)
	{
		throw std::runtime_error("Graphics queue cannot support presentation!");
	}

	const VkQueueFamilyProperties qf = families[m_graphicsQueueFamilyIndex];

	std::cout << "Queue family index: " << m_graphicsQueueFamilyIndex << std::endl;
	std::cout << "Queue count: " << qf.queueCount << std::endl;
	std::cout << "Queue flags: " << qf.queueFlags << std::endl;

	if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)  std::cout << "    - Graphics" << std::endl;
	if (qf.queueFlags & VK_QUEUE_COMPUTE_BIT)   std::cout << "    - Compute" << std::endl;
	if (qf.queueFlags & VK_QUEUE_TRANSFER_BIT)  std::cout << "    - Transfer" << std::endl;
	if (qf.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) std::cout << "    - Sparse binding" << std::endl;
}

void VulkanContext::createLogicalDevice()
{
	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &priority;

	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;
	features.sampleRateShading = VK_TRUE;

	// Enable dynamic rendering
	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
	dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	// Enable syncronization2 
	VkPhysicalDeviceSynchronization2Features synchronization2Features{};
	synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	synchronization2Features.synchronization2 = VK_TRUE;

	// Link the structs together
	dynamicRenderingFeatures.pNext = &synchronization2Features;

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &dynamicRenderingFeatures;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &features;
	deviceCreateInfo.enabledExtensionCount = 1;

	const char* deviceExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	deviceCreateInfo.ppEnabledExtensionNames = &deviceExt;

	if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device!");
	}

	vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

	std::cout << "Logical device and graphics queue created successfully" << std::endl;
}

void VulkanContext::createAllocator()
{
	// Set up VMA Vulkan function pointers for Volk
	VmaVulkanFunctions vulkanFunctions{};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	// Other functions can be loaded automatically using these

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_physicalDevice;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = m_instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan Memory Allocator");
	};
}
