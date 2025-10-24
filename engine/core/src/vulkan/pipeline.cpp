#include "vulkan/pipeline.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "vulkan/render_resources.hpp"
#include "fge/utils.hpp"
#include "fge/logger.hpp"
#include <array>
#include <numeric>
#include <cstring>

namespace fge
{
namespace render
{
namespace
{

#pragma pack(push, 4)
struct DefaultPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};

struct QuadPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};

struct SpritePushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
  Vec2f spriteUvCoords[4];    // 32 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};

struct DynamicTextPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
  uint32_t text[8];           // 32 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};
#pragma pack(pop)

// TODO: Store all of these details in the ShaderProgram object

constexpr size_t DEFAULT_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t DEFAULT_PUSH_CONSTANTS_VERT_SIZE = 64;
constexpr size_t DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET = 64;
constexpr size_t DEFAULT_PUSH_CONSTANTS_FRAG_SIZE = 16;

constexpr size_t SPRITE_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t SPRITE_PUSH_CONSTANTS_VERT_SIZE = 96;
constexpr size_t SPRITE_PUSH_CONSTANTS_FRAG_OFFSET = 96;
constexpr size_t SPRITE_PUSH_CONSTANTS_FRAG_SIZE = 16;

constexpr size_t QUAD_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t QUAD_PUSH_CONSTANTS_VERT_SIZE = 64;
constexpr size_t QUAD_PUSH_CONSTANTS_FRAG_OFFSET = 64;
constexpr size_t QUAD_PUSH_CONSTANTS_FRAG_SIZE = 16;

constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_SIZE = 96;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET = 96;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_SIZE = 16;

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

VkPipelineInputAssemblyStateCreateInfo defaultInputAssemblyState()
{
  return VkPipelineInputAssemblyStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };
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

VkPipelineDepthStencilStateCreateInfo disabledDepthStencilState()
{
  return VkPipelineDepthStencilStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = VK_FALSE,
    .depthWriteEnable = VK_FALSE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
    .front = {},
    .back = {},
    .minDepthBounds = 0.f,
    .maxDepthBounds = 1.f
  };
}

class PipelineImpl : public Pipeline
{
  public:
    PipelineImpl(const ShaderProgramSpec& spec, const ShaderProgram& shader,
      const RenderResources& renderResources, Logger& logger, VkDevice device,
      VkExtent2D swapchainExtent, VkFormat swapchainImageFormat, VkFormat depthFormat,
      const ScreenMargins& margins);

    void onViewportResize(VkExtent2D swapchainExtent) override;

    void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame) override;

    ~PipelineImpl() override;

  private:
    Logger& m_logger;
    const RenderResources& m_renderResources;
    ShaderProgramSpec m_spec;
    VkDevice m_device;
    VkFormat m_swapchainImageFormat;
    ScreenMargins m_margins;
    VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo m_vertShaderStageInfo;
    VkPipelineShaderStageCreateInfo m_fragShaderStageInfo;
    std::vector<VkVertexInputAttributeDescription> m_vertexAttributeDescriptions;
    std::vector<VkVertexInputBindingDescription> m_vertexBindingDescriptions;
    VkPipelineVertexInputStateCreateInfo m_vertexInputStateInfo;
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<VkPushConstantRange> m_pushConstantRanges;
    VkPipelineLayoutCreateInfo m_layoutInfo;
    VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyStateInfo;
    VkViewport m_viewport;
    VkRect2D m_initialScissor;
    VkExtent2D m_swapchainExtent;
    VkPipelineRasterizationStateCreateInfo m_rasterizationStateInfo;
    VkPipelineMultisampleStateCreateInfo m_multisampleStateInfo;
    VkPipelineColorBlendAttachmentState m_colourBlendAttachmentState;
    VkPipelineColorBlendStateCreateInfo m_colourBlendStateInfo;
    VkPipelineDepthStencilStateCreateInfo m_depthStencilStateInfo;
    VkPipelineRenderingCreateInfo m_renderingCreateInfo;

    void constructPipeline(VkExtent2D swapchainExtent);
    void destroyPipeline();
};

PipelineImpl::PipelineImpl(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  const RenderResources& renderResources, Logger& logger, VkDevice device,
  VkExtent2D swapchainExtent, VkFormat swapchainImageFormat, VkFormat depthFormat,
  const ScreenMargins& margins)
  : m_logger(logger)
  , m_renderResources(renderResources)
  , m_spec(spec)
  , m_device(device)
  , m_swapchainImageFormat(swapchainImageFormat)
  , m_margins(margins)
{
  m_vertShaderModule = createShaderModule(m_device, shader.vertexShaderCode);
  m_fragShaderModule = createShaderModule(m_device, shader.fragmentShaderCode);

  m_vertShaderStageInfo = VkPipelineShaderStageCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = m_vertShaderModule,
    .pName = "main",
    .pSpecializationInfo = nullptr
  };

  m_fragShaderStageInfo = VkPipelineShaderStageCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = m_fragShaderModule,
    .pName = "main",
    .pSpecializationInfo = nullptr
  };

  size_t vertexSize = 0;
  for (auto usage : spec.meshFeatures.vertexLayout) {
    vertexSize += getAttributeSize(usage);
  }

  VkVertexInputBindingDescription vertexBindingDescription{
    .binding = 0,
    .stride = static_cast<uint32_t>(vertexSize),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
  };

  m_vertexAttributeDescriptions = createAttributeDescriptions(spec.meshFeatures.vertexLayout);
  if (spec.meshFeatures.flags.test(MeshFeatures::IsInstanced)) {
    for (unsigned int i = 0; i < 4; ++i) {
      uint32_t offset = offsetof(MeshInstance, modelMatrix) + 4 * sizeof(float) * i;
  
      VkVertexInputAttributeDescription attr{
        .location = (LAST_ATTR_IDX - static_cast<uint32_t>(BufferUsage::AttrPosition)) + 1 + i,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = offset
      };
  
      m_vertexAttributeDescriptions.push_back(attr);
    }
  }

  m_vertexBindingDescriptions = {
    vertexBindingDescription
  };
  if (spec.meshFeatures.flags.test(MeshFeatures::IsInstanced)) {
    m_vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{
      .binding = 1,
      .stride = sizeof(MeshInstance),
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
    });
  }

  m_vertexInputStateInfo = VkPipelineVertexInputStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindingDescriptions.size()),
    .pVertexBindingDescriptions = m_vertexBindingDescriptions.data(),
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributeDescriptions.size()),
    .pVertexAttributeDescriptions = m_vertexAttributeDescriptions.data()
  };

  m_inputAssemblyStateInfo = defaultInputAssemblyState();
  bool doubleSided = spec.materialFeatures.flags.test(MaterialFeatures::IsDoubleSided);
  m_rasterizationStateInfo = defaultRasterizationState(doubleSided);
  if (m_spec.renderPass == RenderPass::Shadow) {
    m_rasterizationStateInfo.depthBiasEnable = VK_TRUE;
    m_rasterizationStateInfo.depthClampEnable = VK_FALSE;
    m_rasterizationStateInfo.depthBiasClamp = 0.f;
    m_rasterizationStateInfo.depthBiasConstantFactor = 0.f;
    m_rasterizationStateInfo.depthBiasSlopeFactor = 0.1f;
    m_rasterizationStateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
  }
  m_multisampleStateInfo = defaultMultisamplingState();
  m_colourBlendStateInfo = defaultColourBlendState(m_colourBlendAttachmentState);
  m_depthStencilStateInfo = disabledDepthStencilState();// defaultDepthStencilState();

  m_descriptorSetLayouts = {
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Global),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::RenderPass),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Material),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Object)
  };

  if (spec.meshFeatures.flags.test(MeshFeatures::IsQuad)) {
    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      m_pushConstantRanges = {
        VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = SPRITE_PUSH_CONSTANTS_VERT_OFFSET,
          .size = SPRITE_PUSH_CONSTANTS_VERT_SIZE
        },
        VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = SPRITE_PUSH_CONSTANTS_FRAG_OFFSET,
          .size = SPRITE_PUSH_CONSTANTS_FRAG_SIZE
        }
      };
    }
    else {
      m_pushConstantRanges = {
        VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = QUAD_PUSH_CONSTANTS_VERT_OFFSET,
          .size = QUAD_PUSH_CONSTANTS_VERT_SIZE
        },
        VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = QUAD_PUSH_CONSTANTS_FRAG_OFFSET,
          .size = QUAD_PUSH_CONSTANTS_FRAG_SIZE
        }
      };
    }
  }
  else if (spec.meshFeatures.flags.test(MeshFeatures::IsDynamicText)) {
    m_pushConstantRanges = {
      VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_OFFSET,
        .size = DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_SIZE
      },
      VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET,
        .size = DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_SIZE
      }
    };
  }
  else if (!m_spec.meshFeatures.flags.test(MeshFeatures::IsInstanced)
    && !m_spec.meshFeatures.flags.test(MeshFeatures::IsSkybox)) {

    m_pushConstantRanges = {
      VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = DEFAULT_PUSH_CONSTANTS_VERT_OFFSET,
        .size = DEFAULT_PUSH_CONSTANTS_VERT_SIZE
      },
      VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET,
        .size = DEFAULT_PUSH_CONSTANTS_FRAG_SIZE
      }
    };
  }

  m_layoutInfo = VkPipelineLayoutCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size()),
    .pSetLayouts = m_descriptorSetLayouts.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size()),
    .pPushConstantRanges = m_pushConstantRanges.size() == 0 ? nullptr : m_pushConstantRanges.data()
  };

  switch (m_spec.renderPass) {
    case RenderPass::Overlay:
      m_renderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_swapchainImageFormat,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
      };
      break;
    case RenderPass::Main:
      m_renderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_swapchainImageFormat,
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
      };
      break;
    case RenderPass::Shadow:
      m_renderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
      };
      break;
  }

  constructPipeline(swapchainExtent);
}

void PipelineImpl::constructPipeline(VkExtent2D swapchainExtent)
{
  destroyPipeline();

  m_swapchainExtent = swapchainExtent;

  VK_CHECK(vkCreatePipelineLayout(m_device, &m_layoutInfo, nullptr, &m_layout),
    "Failed to create default pipeline layout");

  m_logger.info(STR("Pipeline layout: " << m_layout));
  m_logger.info(STR("Render pass overlay: " << (m_spec.renderPass == RenderPass::Overlay)));
  m_logger.info(STR("Render pass main: " << (m_spec.renderPass == RenderPass::Main)));
  m_logger.info(STR("Swapchain extent: "
    << m_swapchainExtent.width << ", " << m_swapchainExtent.height));

  VkPipelineShaderStageCreateInfo shaderStages[] = {
    m_vertShaderStageInfo,
    m_fragShaderStageInfo
  };

  Rectf rect{
    .x = static_cast<float>(m_margins.left),
    .y = static_cast<float>(m_margins.top),
    .w = static_cast<float>(swapchainExtent.width - m_margins.left - m_margins.right),
    .h = static_cast<float>(swapchainExtent.height - m_margins.top - m_margins.bottom)
  };

  auto viewportStateInfo = defaultViewportState(m_viewport, m_initialScissor, m_swapchainExtent,
    rect);

  std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamicState{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .dynamicStateCount = 1,
    .pDynamicStates = dynamicStates.data()
  };

  VkGraphicsPipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &m_renderingCreateInfo,
    .flags = 0,
    .stageCount = 2,
    .pStages = shaderStages,
    .pVertexInputState = &m_vertexInputStateInfo,
    .pInputAssemblyState = &m_inputAssemblyStateInfo,
    .pTessellationState = nullptr,
    .pViewportState = &viewportStateInfo,
    .pRasterizationState = &m_rasterizationStateInfo,
    .pMultisampleState = &m_multisampleStateInfo,
    .pDepthStencilState = &m_depthStencilStateInfo,
    .pColorBlendState = &m_colourBlendStateInfo,
    .pDynamicState = &dynamicState,
    .layout = m_layout,
    .renderPass = nullptr,
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1
  };

  VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
    &m_pipeline), "Failed to create default pipeline");

  m_logger.info(STR("Pipeline: " << m_pipeline));
}

void PipelineImpl::onViewportResize(VkExtent2D swapchainExtent)
{
  constructPipeline(swapchainExtent);
}

// TODO: Refactor
void PipelineImpl::recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
  BindState& bindState, size_t currentFrame)
{
  auto globalDescriptorSet = m_renderResources.getGlobalDescriptorSet(currentFrame);
  auto renderPassDescriptorSet = m_renderResources.getRenderPassDescriptorSet(
    m_spec.renderPass, currentFrame);
  auto materialDescriptorSet = m_renderResources.getMaterialDescriptorSet(node.material.id);
  auto objectDescriptorSet = m_renderResources.getObjectDescriptorSet(node.mesh.id, currentFrame);

  auto buffers = m_renderResources.getMeshBuffers(node.mesh.id);

  if (m_pipeline != bindState.pipeline) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  }
  std::vector<VkBuffer> vertexBuffers{ buffers.vertexBuffer };
  if (node.mesh.features.flags.test(MeshFeatures::IsInstanced)) {
    vertexBuffers.push_back(buffers.instanceBuffer);
  }
  std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
  vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(vertexBuffers.size()),
    vertexBuffers.data(), offsets.data());
  vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

  std::vector<VkDescriptorSet> descriptorSets{
    globalDescriptorSet,
    renderPassDescriptorSet
  };
  if (materialDescriptorSet != VK_NULL_HANDLE) {
    descriptorSets.push_back(materialDescriptorSet);
  }
  if (objectDescriptorSet != VK_NULL_HANDLE) {
    descriptorSets.push_back(objectDescriptorSet);
  }

  //m_logger.info(STR("Global descriptor set: " << globalDescriptorSet));
  //m_logger.info(STR("Render pass descriptor set: " << renderPassDescriptorSet));
  //m_logger.info(STR("Material descriptor set: " << materialDescriptorSet));
  //m_logger.info(STR("Object descriptor set: " << objectDescriptorSet));

  //if (descriptorSets != bindState.descriptorSets) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0,
      static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
  //}

  if (node.type == RenderNodeType::DefaultModel) {
    assert(!node.mesh.features.flags.test(MeshFeatures::IsInstanced));
    assert(!node.mesh.features.flags.test(MeshFeatures::IsSkybox));

    auto& defaultNode = dynamic_cast<const DefaultModelNode&>(node);

    DefaultPushConstants constants{
      .modelMatrix = defaultNode.modelMatrix,
      .colour = defaultNode.colour
    };

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
      DEFAULT_PUSH_CONSTANTS_VERT_OFFSET, DEFAULT_PUSH_CONSTANTS_VERT_SIZE, &constants);

    void* fragPart = reinterpret_cast<char*>(&constants) + DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET;

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
      DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET, DEFAULT_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
  }
  else if (node.type == RenderNodeType::Sprite) {
    auto& spriteNode = dynamic_cast<const SpriteNode&>(node);

    SpritePushConstants constants{
      .modelMatrix = spriteNode.modelMatrix,
      .spriteUvCoords = {
        spriteNode.uvCoords[0],
        spriteNode.uvCoords[1],
        spriteNode.uvCoords[2],
        spriteNode.uvCoords[3]
      },
      .colour = spriteNode.colour
    };

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
      SPRITE_PUSH_CONSTANTS_VERT_OFFSET, SPRITE_PUSH_CONSTANTS_VERT_SIZE, &constants);

    void* fragPart = reinterpret_cast<char*>(&constants) + SPRITE_PUSH_CONSTANTS_FRAG_OFFSET;

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
      SPRITE_PUSH_CONSTANTS_FRAG_OFFSET, SPRITE_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
  }
  else if (node.type == RenderNodeType::Quad) {
    auto& quadNode = dynamic_cast<const QuadNode&>(node);

    QuadPushConstants constants{
      .modelMatrix = quadNode.modelMatrix,
      .colour = quadNode.colour
    };

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
      QUAD_PUSH_CONSTANTS_VERT_OFFSET, QUAD_PUSH_CONSTANTS_VERT_SIZE, &constants);

    void* fragPart = reinterpret_cast<char*>(&constants) + QUAD_PUSH_CONSTANTS_FRAG_OFFSET;

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
      QUAD_PUSH_CONSTANTS_FRAG_OFFSET, QUAD_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
  }
  else if (node.type == RenderNodeType::DynamicText) {
    auto& textNode = dynamic_cast<const DynamicTextNode&>(node);

    DynamicTextPushConstants constants;
    constants.modelMatrix = textNode.modelMatrix;
    constants.colour = textNode.colour;

    memset(constants.text, '\0', sizeof(constants.text));
    strncpy(reinterpret_cast<char*>(constants.text), textNode.text.c_str(), textNode.text.size());

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
      DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_OFFSET, DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_SIZE, &constants);

    void* fragPart = reinterpret_cast<char*>(&constants) + DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET;

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
      DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET, DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
  }

  if (node.type == RenderNodeType::InstancedModel) {
    assert(node.mesh.features.flags.test(MeshFeatures::IsInstanced));

    vkCmdDrawIndexed(commandBuffer, buffers.numIndices, buffers.numInstances, 0, 0, 0);
  }
  else {
    vkCmdDrawIndexed(commandBuffer, buffers.numIndices, 1, 0, 0, 0);
  }

  bindState.pipeline = m_pipeline;
  bindState.descriptorSets = descriptorSets;
}

void PipelineImpl::destroyPipeline()
{
  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
  }
  if (m_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  }
}

PipelineImpl::~PipelineImpl()
{
  if (m_vertShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device, m_vertShaderModule, nullptr);
  }
  if (m_fragShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device, m_fragShaderModule, nullptr);
  }
  destroyPipeline();
}

} // namespace

PipelinePtr createPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shaderProgram,
  const RenderResources& renderResources, Logger& logger, VkDevice device,
  VkExtent2D swapchainExtent, VkFormat swapchainImageFormat, VkFormat depthFormat,
  const ScreenMargins& margins)
{
  return std::make_unique<PipelineImpl>(spec, shaderProgram, renderResources, logger, device,
    swapchainExtent, swapchainImageFormat, depthFormat, margins);
}

} // namespace render
} // namespace fge
