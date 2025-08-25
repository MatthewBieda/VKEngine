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

	VkSurfaceKHR surface = nullptr;
	VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (err)
	{
		std::cout << "Window creation failed" << err << std::endl;
		return -1;
	}

	// Get physical devices
	uint32_t numDevices = 0;
	vkEnumeratePhysicalDevices(instance, &numDevices, nullptr);

	std::vector<VkPhysicalDevice> devices(numDevices);
	vkEnumeratePhysicalDevices(instance, &numDevices, devices.data());

	// Score each device
	VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
	int bestScore = -1;
	for (const VkPhysicalDevice& device: devices)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		int score = 0;

		// Prefer discrete GPUs
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestDevice = device;
		}
	}

	// Enumerate device extensions and ensure swapchain support
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(bestDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(bestDevice, nullptr, &extensionCount, availableExtensions.data());

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
		std::cerr << "Selected device does not support VK_KHR_swapchain!" << std::endl;
		return -1;
	}

	// Configure device extensions
	std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };

	// Enable specific GPU features if we need them
	VkPhysicalDeviceFeatures deviceFeatures{};
	//vkGetPhysicalDeviceFeatures(physDev, &deviceFeatures);

	// Query queue families
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(bestDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(bestDevice, &queueFamilyCount, queueFamilies.data());

	// Find a queue that supports graphics and present
	int graphicsQueueFamilyIndex = -1;
	int presentQueueFamilyIndex = -1;

	for (int i = 0; i < queueFamilies.size(); ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			graphicsQueueFamilyIndex = i;
		}

		// Query presentation support
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(bestDevice, i, surface, &presentSupport);
		if (presentSupport)
		{
			presentQueueFamilyIndex = i;
		}

		// Early exit if both capabilities found in the same family
		if (graphicsQueueFamilyIndex != -1 && presentQueueFamilyIndex != -1)
		{
			break;
		}
	}

	if (graphicsQueueFamilyIndex == -1)
	{
		std::cerr << "No queue family supports graphics!" << std::endl;
		return -1;
	}

	if (presentQueueFamilyIndex == -1)
	{
		std::cerr << "Presentation not supported!" << std::endl;
		return -1;
	}

	if (graphicsQueueFamilyIndex != presentQueueFamilyIndex)
	{
		std::cerr << "Graphics queue cannot support presentation" << std::endl;
		return -1;
	}

	const VkQueueFamilyProperties qf = queueFamilies[graphicsQueueFamilyIndex];

	std::cout << "Queue family index: " << graphicsQueueFamilyIndex << std::endl;
	std::cout << "Queue count: " << qf.queueCount << std::endl;
	std::cout << "Queue flags: " << qf.queueFlags << std::endl;

	if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)  std::cout << "    - Graphics" << std::endl;
	if (qf.queueFlags & VK_QUEUE_COMPUTE_BIT)   std::cout << "    - Compute" << std::endl;
	if (qf.queueFlags & VK_QUEUE_TRANSFER_BIT)  std::cout << "    - Transfer" << std::endl;
	if (qf.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) std::cout << "    - Sparse binding" << std::endl;

	// Specify queue creation info using the right family
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// Specify logical device create info
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Create a logical device
	VkDevice device = VK_NULL_HANDLE;
	if (vkCreateDevice(bestDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		std::cerr << "Failed to create logical device!" << std::endl;
		return -1;
	}

	// Get the graphics queue (assume queue 0 from family 0)
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

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