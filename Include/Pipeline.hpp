#pragma once
#include <string>
#include <vector>
#include <array>

#include "volk.h"
#include "glm.hpp"

class VulkanContext;
class Swapchain;
class DescriptorManager;

enum class PipelineType
{
	Scene,
	Skybox
};

class Pipeline
{
public:
	Pipeline(VulkanContext& context, Swapchain& swapchain, DescriptorManager& descriptors, const std::string& vertPath, const std::string& fragPath, VkFormat depthFormat, PipelineType type = PipelineType::Scene);
	~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(Pipeline&&) = delete;
	Pipeline& operator=(Pipeline&&) = delete;

	void setViewport(VkCommandBuffer cmdBuffer, VkViewport viewport);
	void setScissor(VkCommandBuffer cmdBuffer, VkRect2D scissor);
	void setDepthTest(VkCommandBuffer cmdBuffer, VkBool32 depthTestEnable);
	void setPolygonMode(VkCommandBuffer cmdBuffer, VkPolygonMode polygonMode);
	void setCullMode(VkCommandBuffer cmdBuffer, VkCullModeFlags cullMode);

	VkPipeline getPipeline() const { return m_pipeline; }
	VkPipelineLayout getLayout() const { return m_layout; }

private:
	VkShaderModule createShaderModule(const std::vector<char>& code);
	std::vector<char> readFile(const std::string& filename);
	void createPipeline(const std::string& vertPath, const std::string& fragPath, VkFormat colorFormat, VkFormat depthFormat, PipelineType type);

	VulkanContext& m_context;
	Swapchain& m_swapchain;
	DescriptorManager& m_descriptors;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	static constexpr float DEFAULT_LINE_WIDTH = 1.0f;
	static constexpr VkSampleCountFlagBits DEFAULT_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
};
