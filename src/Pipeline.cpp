#include "Utils.hpp"

#include "Pipeline.hpp"
#include "VulkanContext.hpp" 
#include "Swapchain.hpp"
#include "DescriptorManager.hpp"
#include "Vertex.hpp"
#include "DebugVertex.hpp"

#include <fstream>
#include <iostream>
#include <array>

Pipeline::Pipeline(VulkanContext& context, Swapchain& swapchain, DescriptorManager& descriptors, uint32_t pushConstantsSize, const std::string& vertPath, const std::string& fragPath, VkFormat depthFormat, PipelineType type)
	: m_context(context), m_swapchain(swapchain), m_descriptors(descriptors)
{
	createPipeline(vertPath, fragPath, m_swapchain.getFormat(), depthFormat, type, pushConstantsSize);
}

Pipeline::~Pipeline()
{
	vkDestroyPipeline(m_context.getDevice(), m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_context.getDevice(), m_layout, nullptr);
}

void Pipeline::setViewport(VkCommandBuffer cmdBuffer, VkViewport viewport)
{
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
}

void Pipeline::setScissor(VkCommandBuffer cmdBuffer, VkRect2D scissor)
{
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
}

void Pipeline::setDepthTest(VkCommandBuffer cmdBuffer, VkBool32 depthTestEnable)
{
	vkCmdSetDepthTestEnable(cmdBuffer, depthTestEnable);
}

void Pipeline::setPolygonMode(VkCommandBuffer cmdBuffer, VkPolygonMode polygonMode)
{
	vkCmdSetPolygonModeEXT(cmdBuffer, polygonMode);
}

void Pipeline::setCullMode(VkCommandBuffer cmdBuffer, VkCullModeFlags cullMode)
{
	vkCmdSetCullMode(cmdBuffer, cullMode);
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_context.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}
	return shaderModule;

}

std::vector<char> Pipeline::readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

void Pipeline::createPipeline(const std::string& vertPath, const std::string& fragPath, VkFormat colorFormat, VkFormat depthFormat, PipelineType type, uint32_t pushConstantsSize)
{
	std::vector<char> vertCode = readFile(vertPath);
	std::vector<char> fragCode = readFile(fragPath);

	VkShaderModule vertModule = createShaderModule(vertCode);
	VkShaderModule fragModule = createShaderModule(fragCode);

	VkPipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = vertModule;
	vertStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = fragModule;
	fragStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

	// Modern vulkan supports dynamic modification of some pipeline states
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
		VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
		VK_DYNAMIC_STATE_CULL_MODE
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// Fixed-function state (vertex input, input assembly, viewport, rasterizer, multisample, color blend)
	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = Vertex::getAttributeDescription();

	VkVertexInputBindingDescription debugBinding;
	std::array<VkVertexInputAttributeDescription, 2> debugAttributes;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = nullptr;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = nullptr;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.depthBiasClamp = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = DEFAULT_LINE_WIDTH;
	rasterizerInfo.cullMode = VK_CULL_MODE_NONE; 
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.alphaToCoverageEnable = VK_TRUE;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = DEFAULT_SAMPLES;

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

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = pushConstantsSize;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;

	VkDescriptorSetLayout descriptorSetLayout = m_descriptors.getDescriptorSetLayout();
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}
	std::cout << "Pipeline layout created successfully" << std::endl;
	nameObject(m_context.getDevice(), m_layout, "PipelineLayout_Main");

	// Create graphics pipeline with dynamic rendering support

	// Set stencil format based on whether the depth format includes stencil
	VkFormat stencilFormat = (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
							  depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) ?
							  depthFormat : VK_FORMAT_UNDEFINED;

	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	VkFormat colorAttachmentFormat = colorFormat;
	renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
	renderingInfo.depthAttachmentFormat = depthFormat;
	renderingInfo.stencilAttachmentFormat = stencilFormat;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = stencilFormat == depthFormat ? VK_TRUE : VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;

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
	graphicsPipelineInfo.pDepthStencilState = &depthStencil;
	graphicsPipelineInfo.layout = m_layout;
	graphicsPipelineInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineInfo.subpass = 0;

	switch (type)
	{
		case PipelineType::Scene:
			// Regular mesh pipeline uses default configuration
			break;

		case PipelineType::Skybox:
			// Skybox should be rendered behind everything and visible from inside the cube
			rasterizerInfo.cullMode = VK_CULL_MODE_FRONT_BIT; // Draw inside of cube
			depthStencil.depthWriteEnable = VK_FALSE; // Don't overwrite scene depth
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = nullptr;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = nullptr;
			break;

		case PipelineType::Transparent:
			// Disable depth writes, but keep depth testing
			depthStencil.depthWriteEnable = VK_FALSE;

			// Disable backface culling for glass
			rasterizerInfo.cullMode = VK_CULL_MODE_NONE;

			// Disable alpha to coverage (used for foliage, not smooth transparency)
			multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
			break;

		case PipelineType::DebugAABB:
			// Retrieve the debug data using pre-declared variables
			debugBinding = DebugVertex::getBindingDescription();
			debugAttributes = DebugVertex::getAttributeDescription();

			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &debugBinding;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(debugAttributes.size());
			vertexInputInfo.pVertexAttributeDescriptions = debugAttributes.data();

			inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			rasterizerInfo.polygonMode = VK_POLYGON_MODE_LINE;
			rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_FALSE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			colorBlendAttachment.blendEnable = VK_FALSE;
	}

	if (vkCreateGraphicsPipelines(m_context.getDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline");
	}

	switch (type)
	{
		case PipelineType::Scene:
			std::cout << "Graphics Pipeline (Scene) created successfully" << std::endl;
			nameObject(m_context.getDevice(), m_pipeline, "GraphicsPipeline_Scene");
			break;

		case PipelineType::Skybox:
			std::cout << "Graphics Pipeline (Skybox) created successfully" << std::endl;
			nameObject(m_context.getDevice(), m_pipeline, "GraphicsPipeline_Skybox");
			break;

		case PipelineType::Transparent:
			std::cout << "Graphics Pipeline (Transparent) created successfully" << std::endl;
			nameObject(m_context.getDevice(), m_pipeline, "GraphicsPipeline_Transparent");
			break;

		case PipelineType::DebugAABB:
			std::cout << "Graphics Pipeline (DebugAABB) created successfully" << std::endl;
			nameObject(m_context.getDevice(), m_pipeline, "GraphicsPipeline_DebugAABB");
			break;
	}

	// Destory shader modules after pipeline creation
	vkDestroyShaderModule(m_context.getDevice(), vertModule, nullptr);
	vkDestroyShaderModule(m_context.getDevice(), fragModule, nullptr);
}
