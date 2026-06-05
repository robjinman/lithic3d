#include "vulkan/pipelines/pipeline_utils.hpp"
#include "vulkan/vulkan_utils.hpp"

namespace lithic3d
{
namespace render
{

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code)
{
  VkShaderModuleCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .codeSize = code.size() * sizeof(uint32_t),
    .pCode = code.data()
  };

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
    "Failed to create shader module");

  return shaderModule;
}

VkFormat attributeFormat(BufferUsage usage)
{
  switch (usage) {
    case BufferUsage::AttrPosition: return VK_FORMAT_R32G32B32_SFLOAT;
    case BufferUsage::AttrNormal: return VK_FORMAT_R32G32B32_SFLOAT;
    case BufferUsage::AttrColour: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case BufferUsage::AttrTexCoord: return VK_FORMAT_R32G32_SFLOAT;
    case BufferUsage::AttrTangent: return VK_FORMAT_R32G32B32_SFLOAT;
    case BufferUsage::AttrJointIndices: return VK_FORMAT_R8G8B8A8_UINT;
    case BufferUsage::AttrJointWeights: return VK_FORMAT_R32G32B32A32_SFLOAT;
    default: EXCEPTION("Buffer type is not a vertex attribute");
  }
}

std::vector<VkVertexInputAttributeDescription>
  createAttributeDescriptions(const VertexLayout& layout)
{
  std::vector<VkVertexInputAttributeDescription> attributes;

  const uint32_t first = static_cast<uint32_t>(BufferUsage::AttrPosition);
  for (auto& attribute : layout) {
    if (attribute == BufferUsage::None) {
      break;
    }

    attributes.push_back(VkVertexInputAttributeDescription{
      .location = static_cast<uint32_t>(attribute) - first,
      .binding = 0,
      .format = attributeFormat(attribute),
      .offset = static_cast<uint32_t>(calcOffsetInVertex(layout, attribute))
    });
  }

  return attributes;
}

VkPipelineViewportStateCreateInfo defaultViewportState(VkViewport& viewport, VkRect2D& scissor,
  VkExtent2D swapchainExtent, const Rectf& viewportRect)
{
  viewport = VkViewport{
    .x = viewportRect.x,
    .y = viewportRect.y,
    .width = viewportRect.w,
    .height = viewportRect.h,
    .minDepth = 0.f,
    .maxDepth = 1.f
  };

  scissor = VkRect2D{
    .offset = { 0, 0 },
    .extent = swapchainExtent
  };

  return VkPipelineViewportStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor
  };
}

VkPipelineRasterizationStateCreateInfo defaultRasterizationState(bool doubleSided)
{
  VkCullModeFlags cullMode = doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
  return VkPipelineRasterizationStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = cullMode,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f
  };
}

VkPipelineMultisampleStateCreateInfo defaultMultisamplingState()
{
  return VkPipelineMultisampleStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 1.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE
  };
}

VkPipelineColorBlendStateCreateInfo
defaultColourBlendState(VkPipelineColorBlendAttachmentState& colourBlendAttachment)
{
  colourBlendAttachment = VkPipelineColorBlendAttachmentState{
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                      VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT |
                      VK_COLOR_COMPONENT_A_BIT
  };

  return VkPipelineColorBlendStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_AND,
    .attachmentCount = 1,
    .pAttachments = &colourBlendAttachment,
    .blendConstants = { 0, 0, 0, 0 }
  };
}

VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState()
{
  return VkPipelineDepthStencilStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
    .front = {},
    .back = {},
    .minDepthBounds = 0.f,
    .maxDepthBounds = 1.f
  };
}

} // namespace render
} // namespace lithic3d
