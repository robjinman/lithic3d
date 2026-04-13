#pragma once

#include "lithic3d/exception.hpp"
#include <vulkan/vulkan.h>

namespace lithic3d
{

extern PFN_vkCmdBeginRendering vkCmdBeginRenderingFn;
extern PFN_vkCmdEndRendering vkCmdEndRenderingFn;

void loadVulkanExtensionFunctions(VkInstance instance);

#define VK_CHECK(fnCall, msg) \
  { \
    VkResult MAC_code = fnCall; \
    if (MAC_code != VK_SUCCESS) { \
      EXCEPTION(msg << " (result: " << MAC_code << ")"); \
    } \
  }

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format,
  VkImageAspectFlags aspectFlags, VkImageViewType type, uint32_t layerCount, uint32_t layer);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
  VkMemoryPropertyFlags properties);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

} // namespace lithic3d
