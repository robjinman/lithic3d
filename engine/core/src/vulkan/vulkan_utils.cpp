#include "vulkan/vulkan_utils.hpp"
#include "lithic3d/strings.hpp"
#include <vector>

namespace lithic3d
{

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
  VkMemoryPropertyFlags properties)
{
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
    if (typeFilter & (1 << i) &&
      (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {

      return i;
    }
  }

  EXCEPTION("Failed to find suitable memory type");
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format,
  VkImageAspectFlags aspectFlags, VkImageViewType type, uint32_t layerCount, uint32_t layer)
{
  VkImageViewCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .image = image,
    .viewType = type,
    .format = format,
    .components = VkComponentMapping{
      .r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .a = VK_COMPONENT_SWIZZLE_IDENTITY
    },
    .subresourceRange = VkImageSubresourceRange{
      .aspectMask = aspectFlags,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = layer,
      .layerCount = layerCount
    }
  };

  VkImageView imageView;

  VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView),
    "Failed to create image view");

  return imageView;
}

// TODO: Rationalise this
VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
{
  auto findSupportedFormat = [=](const std::vector<VkFormat>& candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features) {

    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return format;
      }
      else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
        (props.optimalTilingFeatures & features) == features) {

        return format;
      }
    }

    EXCEPTION("Failed to find supported format");
  };

  return findSupportedFormat({
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
  }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

} // namespace lithic3d
