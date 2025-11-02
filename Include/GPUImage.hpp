#pragma once

#include "volk.h"
#include "VulkanContext.hpp"

#include <string>

struct ShadowMap
{
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent2D extent{};

	VkImageView view = VK_NULL_HANDLE; // Primary view (for Attachment)
	VkImageView debugView = VK_NULL_HANDLE; // Debug view (for ImGui sampling)
};

class Commands;

class GPUImage
{
public:
	GPUImage(VulkanContext& context, Commands& commands);
	~GPUImage();

	// Load a texture and return its index in the bindless array
	uint32_t loadTexture(const std::string& path, bool is_srgb);

	// Get all texture views for descriptor update
	const std::vector<VkImageView>& getTextureViews() const { return m_textureViews; }
	VkSampler getSampler() const { return m_sharedTextureSampler; }

	// Special images stay separate
	void createDepthImage(uint32_t width, uint32_t height);
	void createMSAAColorImage(uint32_t width, uint32_t height, VkFormat colorFormat);
	void createCubemap(const std::array<std::string, 6>& facePaths);

	void createShadowMap(uint32_t width, uint32_t height, VkFormat = VK_FORMAT_D32_SFLOAT);
	void createShadowSampler();
	VkSampler getShadowSampler() const { return m_shadowSampler; }
	const std::vector<ShadowMap>& getShadowMaps() const { return m_shadowMaps; }

	VkImageView getDepthImageView() const { return m_depthImageView; }
	VkImage getDepthImage() const { return m_depthImage; }
	VkFormat getDepthFormat() const { return m_depthFormat; }
	void recreateDepthImage(uint32_t width, uint32_t height);
	void cleanupDepthResources();

	VkImageView getMSAAColorImageView() const { return m_msaaColorImageView; }
	VkSampleCountFlagBits getMSAASamples() const { return m_msaaSamples; }
	void recreateMSAAColorImage(uint32_t width, uint32_t height, VkFormat colorFormat);
	void cleanupMSAAResources();

	VkImage getSkyboxImage() const { return m_skyboxImage; }
	VkImageView getSkyboxImageView() const { return m_skyboxImageView; }

private:
	VulkanContext& m_context;
	Commands& m_commands;

	struct Texture
	{
		VkImage image;
		VkImageView view;
		VmaAllocation allocation;
		uint32_t mipLevels;
	};

	std::vector<Texture> m_textures;
	std::vector<VkImageView> m_textureViews;
	VkSampler m_sharedTextureSampler = VK_NULL_HANDLE;

	std::vector<ShadowMap> m_shadowMaps; // For cascaded shadow maps
	VkSampler m_shadowSampler = VK_NULL_HANDLE;

	// Helper to load a single texture
	GPUImage::Texture createTextureImageFromFile(const std::string& path, bool is_srgb);

	// Depth resources (multisampled)
	VkImage m_depthImage = VK_NULL_HANDLE;
	VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
	VkImageView m_depthImageView = VK_NULL_HANDLE;
	VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

	// MSAA resources
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_4_BIT;
	VkImage m_msaaColorImage = VK_NULL_HANDLE;
	VmaAllocation m_msaaColorImageAllocation = VK_NULL_HANDLE;
	VkImageView m_msaaColorImageView = VK_NULL_HANDLE;

	// Skybox resources
	VkImage m_skyboxImage = VK_NULL_HANDLE;
	VmaAllocation m_skyboxImageAllocation = VK_NULL_HANDLE;
	VkImageView m_skyboxImageView = VK_NULL_HANDLE;

	void generateMipmaps(VkCommandBuffer cmd, VkImage image, uint32_t mipLevels, uint32_t width, uint32_t height);
	void transitionImageLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout, VkImage textureImage, VkImageAspectFlags aspectMask, uint32_t baseMipLevel = 0, uint32_t mipLevelCount = 1, uint32_t baseArrayLayer = 0, uint32_t arrayLayerCount = 1);
	void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView& outview, uint32_t mipLevels = 1);
	void createSampler();

	VkFormat findSupportedDepthFormat();
	bool hasStencil(VkFormat format) const;
};
