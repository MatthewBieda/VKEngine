#include <iostream>
#include <vector>

#define VOLK_IMPLEMENTATION
#include "volk.h"

#include "glfw3.h"

// Validation layer callback
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

int main()
{
	// Initialize Volk
	if (volkInitialize() != VK_SUCCESS)
	{
		std::cerr << "Failed to initialize Volk!" << std::endl;
		return -1;
	}

	// State application info and API version
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanApp";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VulkanEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> extensions = { "VK_EXT_debug_utils", "VK_KHR_surface", "VK_KHR_win32_surface" };

	// Create a Vulkan instance
	VkInstance instance = VK_NULL_HANDLE;
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		std::cerr << "Failed to create Vulkan instance!" << std::endl;
		return -1;
	}
	std::cout << "Vulkan instance created successfully" << std::endl;

	// After creating Vulkan instance, load functions with Volk
	volkLoadInstance(instance);

	// Setup debug messenger
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
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

	if (vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		std::cerr << "Failed to create debug messenger!" << std::endl;
		return -1;
	}
	std::cout << "Debug messenger created successfully" << std::endl;

	// GLFW window creation
	if (!glfwInit())
	{
		std::cerr << "Failed to initailize GLFW!" << std::endl;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(1920, 1080, "VulkanEngine", nullptr, nullptr);

	VkSurfaceKHR surface = nullptr;;
	VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (err)
	{
		std::cout << "Window creation failed" << err << std::endl;
	}

	// Get physical devices
	uint32_t numDevices = 0;
	vkEnumeratePhysicalDevices(instance, &numDevices, nullptr);

	std::vector<VkPhysicalDevice> devices(numDevices);
	vkEnumeratePhysicalDevices(instance, &numDevices, devices.data());

	// Just use the first device
	VkPhysicalDevice physicalDevice = devices[0];

	// Specify queue creation info (assume queue family 0 exists and supports graphics)
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = 0;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// Specify device features we want
	VkPhysicalDeviceFeatures deviceFeatures{};
	//vkGetPhysicalDeviceFeatures(physDev, &deviceFeatures);

	// Specify logical device create info
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	// Extensions (required for swapchain)	
	std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Create a logical device
	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		std::cerr << "Failed to create logical device!" << std::endl;
		return -1;
	}

	// Get the graphics queue (assume queue 0 from family 0)
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

	// Also load device functions from the driver directly for around 7% speedup
	volkLoadDevice(device);

	std::cout << "Logical device and graphics queue created successfully" << std::endl;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	// Cleanup
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}