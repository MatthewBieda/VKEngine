#include "GPUImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>

GPUImage::GPUImage(VulkanContext& context): m_context(context)
{

}

GPUImage::~GPUImage()
{

}

void GPUImage::createImage()
{
	int texWidth, texHeight, texChannels;
	uint8_t* pixels = stbi_load("../texture/ice.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("Failed to load texture image!");
	}
}