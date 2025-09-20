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

inline void nameObject(VkDevice device, uint64_t handle, VkObjectType type, const char* name)
{

	VkDebugUtilsObjectNameInfoEXT info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = name
	};

	vkSetDebugUtilsObjectNameEXT(device, &info);
}
