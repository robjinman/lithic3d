#pragma once

#include "lithic3d/renderables.hpp"
#include <vulkan/vulkan.h>

namespace lithic3d
{
namespace render
{

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code);

VkFormat attributeFormat(BufferUsage usage);

std::vector<VkVertexInputAttributeDescription>
  createAttributeDescriptions(const VertexLayout& layout);

VkPipelineViewportStateCreateInfo defaultViewportState(VkViewport& viewport, VkRect2D& scissor,
  VkExtent2D swapchainExtent, const Rectf& viewportRect);

VkPipelineRasterizationStateCreateInfo defaultRasterizationState(bool doubleSided);

VkPipelineMultisampleStateCreateInfo defaultMultisamplingState();

VkPipelineColorBlendStateCreateInfo
defaultColourBlendState(VkPipelineColorBlendAttachmentState& colourBlendAttachment);

VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState();

} // namespace render
} // namespace lithic3d
