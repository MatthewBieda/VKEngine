#include <iostream>
#include <fstream>
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

	const int MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t currentFrame = 0;

	// State application info and API version
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanApp";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "VKEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> extensions = { "VK_EXT_debug_utils", "VK_KHR_surface", "VK_KHR_win32_surface" };

	// Create a Vulkan instance
	VkInstance instance = VK_NULL_HANDLE;
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	instanceCreateInfo.ppEnabledLayerNames = layers.data();
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS)
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
	GLFWwindow* window = glfwCreateWindow(1920, 1080, "VKEngine", nullptr, nullptr);

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
	for (const VkPhysicalDevice& device : devices)
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

	// Specify logical device create info
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &dynamicRenderingFeatures;
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

	// Query supported surface formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(bestDevice, surface, &formatCount, nullptr);
	if (formatCount == 0)
	{
		std::cerr << "No surface formats available!" << std::endl;
		return -1;
	}

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(bestDevice, surface, &formatCount, formats.data());

	// Query available present modes
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(bestDevice, surface, &presentModeCount, nullptr);
	if (presentModeCount == 0)
	{
		std::cerr << "No present modes available!" << std::endl;
		return -1;
	}

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(bestDevice, surface, &presentModeCount, presentModes.data());

	// Pick swapchain format
	VkSurfaceFormatKHR chosenFormat = formats[0]; // Fallback if we don't find the desired format
	for (const VkSurfaceFormatKHR& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			chosenFormat = format;
			break;
		}
	}

	// Pick swapchain presentation mode
	VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Fallback to FIFO if MAILBOX not found
	for (const VkPresentModeKHR& presentMode : presentModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			chosenPresentMode = presentMode;
			break;
		}
	}

	// Query surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(bestDevice, surface, &surfaceCapabilities);

	// Create swapchain
	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
	{
		imageCount = surfaceCapabilities.maxImageCount;
	}

	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageFormat = chosenFormat.format;
	swapchainCreateInfo.imageColorSpace = chosenFormat.colorSpace;
	swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = chosenPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain;
	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		std::cerr << "Failed to create swapchain" << std::endl;
		return -1;
	}
	std::cout << "Swapchain created successfully" << std::endl;

	// Retrieve swapchain images
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	std::vector<VkImage> swapChainImages(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	// Image Views
	std::vector<VkImageView> swapChainImageViews(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = chosenFormat.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			std::cerr << "Could not create Image View" << std::endl;
			return -1;
		}
		std::cout << "Image View " << i << " created successfully" << std::endl;
	}

	// Proframmable pipeline setup
	std::string vertexShader{ "../Shaders/vert.spv" };
	std::ifstream vertexFileStream(vertexShader, std::ios::ate | std::ios::binary);

	if (!vertexFileStream.is_open())
	{
		std::cerr << "Failed to open file" << std::endl;
		return -1;
	}

	size_t vertexFileSize = (size_t)vertexFileStream.tellg();
	std::vector<char> vertexBuffer(vertexFileSize);

	vertexFileStream.seekg(0);
	vertexFileStream.read(vertexBuffer.data(), vertexFileSize);
	vertexFileStream.close();

	std::string fragmentShader{ "../Shaders/frag.spv" };
	std::ifstream fragmentFileStream(fragmentShader, std::ios::ate | std::ios::binary);

	if (!fragmentFileStream.is_open())
	{
		std::cerr << "Failed to open file" << std::endl;
		return -1;
	}

	size_t fragmentFileSize = (size_t)fragmentFileStream.tellg();
	std::vector<char> fragmentBuffer(fragmentFileSize);

	fragmentFileStream.seekg(0);
	fragmentFileStream.read(fragmentBuffer.data(), fragmentFileSize);
	fragmentFileStream.close();

	std::cout << "Shaders loaded" << std::endl;

	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo{};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.codeSize = vertexBuffer.size();
	vertexShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertexBuffer.data());

	VkShaderModule vertexShaderModule;
	if (vkCreateShaderModule(device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule) != VK_SUCCESS)
	{
		std::cerr << "Could not create vertex shader module" << std::endl;
		return -1;
	}

	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo{};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.codeSize = fragmentBuffer.size();
	fragmentShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragmentBuffer.data());

	VkShaderModule fragmentShaderModule;
	if (vkCreateShaderModule(device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule) != VK_SUCCESS)
	{
		std::cerr << "Could not create vertex shader module" << std::endl;
		return -1;
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertexShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragmentShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Fixed-function pipeline setup
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)surfaceCapabilities.currentExtent.width;
	viewport.height = (float)surfaceCapabilities.currentExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = surfaceCapabilities.currentExtent;

	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthBiasClamp = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 								  
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.logicOpEnable = VK_FALSE;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorBlendAttachment;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cerr << "Failed to create Pipeline Layout" << std::endl;
		return -1;
	}
	std::cout << "Pipeline layout created successfully" << std::endl;

	// Create graphics pipeline with dynamic rendering support
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	VkFormat colorAttachmentFormat = chosenFormat.format;
	renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;

	VkGraphicsPipelineCreateInfo graphicsPipelineInfo{};
	graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineInfo.pNext = &renderingInfo;
	graphicsPipelineInfo.stageCount = 2;
	graphicsPipelineInfo.pStages = shaderStages;
	graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
	graphicsPipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	graphicsPipelineInfo.pViewportState = &viewportStateInfo;
	graphicsPipelineInfo.pRasterizationState = &rasterizerInfo;
	graphicsPipelineInfo.pMultisampleState = &multisamplingInfo;
	graphicsPipelineInfo.pColorBlendState = &colorBlendInfo;
	graphicsPipelineInfo.pDynamicState = &dynamicStateInfo;
	graphicsPipelineInfo.layout = pipelineLayout;
	graphicsPipelineInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineInfo.subpass = 0;

	VkPipeline graphicsPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		std::cerr << "Failed to create graphics pipeline" << std::endl;
		return -1;
	}
	std::cout << "Graphics Pipeline created successfully" << std::endl;

	// Destory shader modules after pipeline creation
	vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(device, vertexShaderModule, nullptr);

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		std::cerr << "Failed to create command pool" << std::endl;
		return -1;
	}
	std::cout << "Command Pool created successfully" << std::endl;

	std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate command buffers" << std::endl;
		return -1;
	}
	std::cout << "Command Buffer allocated successfully" << std::endl;

	// Create Syncronization primitives
	// Semaphores to signal that an image has been acquired from the swapchain and is ready for rendering
	std::vector<VkSemaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);

	// Semaphores to signal that rendering has finished and presentation can happen
	std::vector<VkSemaphore> renderFinishedSemaphores(swapChainImages.size());

	// Fences to make sure that only one frame is rendering at a time
	std::vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to create syncronization objects for frame: " << i << std::endl;
			return -1;
		}
		std::cout << "Syncronization primitives for frame: " << i << " created successfully" << std::endl;
	}

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
		{
			std::cerr << "Failed to create render finished semaphore: " << i << std::endl;
			return -1;
		}
		std::cout << "Render finished semaphore for frame: " << i << " created successfully" << std::endl;
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo);

		// Transition swapchain image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
		VkImageMemoryBarrier preRenderBarrier{};
		preRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		preRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		preRenderBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		preRenderBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preRenderBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preRenderBarrier.image = swapChainImages[imageIndex];
		preRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		preRenderBarrier.subresourceRange.baseMipLevel = 0;
		preRenderBarrier.subresourceRange.levelCount = 1;
		preRenderBarrier.subresourceRange.baseArrayLayer = 0;
		preRenderBarrier.subresourceRange.layerCount = 1;
		preRenderBarrier.srcAccessMask = 0;
		preRenderBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		vkCmdPipelineBarrier(
			commandBuffers[currentFrame],
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &preRenderBarrier
		);

		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = swapChainImageViews[imageIndex];
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 1.0f} };

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = surfaceCapabilities.currentExtent;
		renderingInfo.layerCount = 1;  // <- required
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		// Start dynamic rendering
		vkCmdBeginRendering(commandBuffers[currentFrame], &renderingInfo);

		// Bind pipeline, set dynamic state, issue draw
		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);
		vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);

		// End rendering
		vkCmdEndRendering(commandBuffers[currentFrame]);

		// Transition swapchain image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
		VkImageMemoryBarrier postRenderBarrier{};
		postRenderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postRenderBarrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		postRenderBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		postRenderBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postRenderBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postRenderBarrier.image = swapChainImages[imageIndex];
		postRenderBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		postRenderBarrier.subresourceRange.baseMipLevel = 0;
		postRenderBarrier.subresourceRange.levelCount = 1;
		postRenderBarrier.subresourceRange.baseArrayLayer = 0;
		postRenderBarrier.subresourceRange.layerCount = 1;
		postRenderBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		postRenderBarrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(
			commandBuffers[currentFrame],
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &postRenderBarrier
		);


		vkEndCommandBuffer(commandBuffers[currentFrame]);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		vkQueuePresentKHR(graphicsQueue, &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// Cleanup

	// Wait for device to finish work
	vkDeviceWaitIdle(device);

	// Cleanup in reverse creation order
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
	}

	vkDestroyCommandPool(device, commandPool, nullptr);

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	for (const auto& imageView : swapChainImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);

	vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	vkDestroyInstance(instance, nullptr);
}
