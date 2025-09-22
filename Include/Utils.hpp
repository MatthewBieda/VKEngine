#pragma once

#include "volk.h"

inline VkDebugUtilsLabelEXT makeLabel(const char* name,
	float r = 1.0f,
	float g = 1.0f,
	float b = 1.0f,
	float a = 1.0f)
{
	VkDebugUtilsLabelEXT label{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext = nullptr,
		.pLabelName = name,
		.color = {r, g, b, a}
	};
	return label;
}

inline void nameObjectRaw(VkDevice device, uint64_t handle, VkObjectType type, const char* name)
{

	VkDebugUtilsObjectNameInfoEXT info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = name
	};

	vkSetDebugUtilsObjectNameEXT(device, &info);
}

// Type-safe overloads
inline void nameObject(VkDevice device, VkBuffer obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_BUFFER, name);
}

inline void nameObject(VkDevice device, VkImage obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_IMAGE, name);
}

inline void nameObject(VkDevice device, VkImageView obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_IMAGE_VIEW, name);
}

inline void nameObject(VkDevice device, VkPipeline obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_PIPELINE, name);
}

inline void nameObject(VkDevice device, VkPipelineLayout obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name);
}

inline void nameObject(VkDevice device, VkShaderModule obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_SHADER_MODULE, name);
}

inline void nameObject(VkDevice device, VkDescriptorSetLayout obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, name);
}

inline void nameObject(VkDevice device, VkDescriptorPool obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_DESCRIPTOR_POOL, name);
}

inline void nameObject(VkDevice device, VkDescriptorSet obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_DESCRIPTOR_SET, name);
}

inline void nameObject(VkDevice device, VkCommandPool obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_COMMAND_POOL, name);
}

inline void nameObject(VkDevice device, VkCommandBuffer obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_COMMAND_BUFFER, name);
}

inline void nameObject(VkDevice device, VkSampler obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_SAMPLER, name);
}

inline void nameObject(VkDevice device, VkSemaphore obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_SEMAPHORE, name);
}

inline void nameObject(VkDevice device, VkFence obj, const char* name) {
	nameObjectRaw(device, (uint64_t)obj, VK_OBJECT_TYPE_FENCE, name);
}
