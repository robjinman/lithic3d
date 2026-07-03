#include "vulkan/pipelines/graphics_pipeline.hpp"
#include "vulkan/pipelines/pipeline_utils.hpp"
#include "vulkan/pipelines/graphics/push_constants.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "vulkan/render_resources.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/trace.hpp"
#include <array>
#include <numeric>
#include <cstring>

namespace lithic3d
{
namespace render
{
namespace
{

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

std::vector<VkPushConstantRange> createPushConstantRanges(const ShaderProgramSpec& spec)
{
  if (spec.meshFeatures.flags.test(MeshFeatures::IsQuad)) {
    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      return {
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
      return {
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
    return {
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
  else {
    return {
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
}

class GfxOverlayPipeline : public GraphicsPipeline
{
  public:
    GfxOverlayPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
      const RenderResources& renderResources, Logger& logger, VkDevice device,
      VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
      const ScreenMargins& margins);

    void onViewportResize(VkExtent2D swapchainExtent) override;

    void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame, uint32_t tick, uint32_t shadowMapCascade) override;

    ~GfxOverlayPipeline() override;

  private:
    Logger& m_logger;
    const RenderResources& m_renderResources;
    ShaderProgramSpec m_spec;
    VkDevice m_device;
    VkRenderPass m_renderPass;
    uint32_t m_subpass;
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

    std::vector<VkDescriptorSet> getDescriptorSets(size_t currentFrame, ResourceId meshId,
      ResourceId materialId) const;
    void drawQuads(VkCommandBuffer commandBuffer, const QuadNode& node, const Mat4x4f& transform);
    void drawDynamicText(VkCommandBuffer commandBuffer, const DynamicTextNode& node,
      const Mat4x4f& transform);
    void drawSprites(VkCommandBuffer commandBuffer, const SpriteNode& node,
      const Mat4x4f& transform);
    void drawDefault(VkCommandBuffer commandBuffer, const DefaultModelNode& node,
      const Mat4x4f& transform);
    void constructGraphicsPipeline(VkExtent2D swapchainExtent);
    void destroyGraphicsPipeline();
};

GfxOverlayPipeline::GfxOverlayPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  const RenderResources& renderResources, Logger& logger, VkDevice device, VkRenderPass renderPass,
  uint32_t subpass, VkExtent2D swapchainExtent, const ScreenMargins& margins)
  : m_logger(logger)
  , m_renderResources(renderResources)
  , m_spec(spec)
  , m_device(device)
  , m_renderPass(renderPass)
  , m_subpass(subpass)
  , m_margins(margins)
{
  ASSERT(spec.renderPass == RenderPass::Overlay, "Pipeline doesn't support render pass");
  // TODO: Support instanced geometry?
  ASSERT(!spec.meshFeatures.flags.test(MeshFeatures::IsInstanced),
    "Pipeline doesn't support instanced geometry");
  ASSERT(!spec.meshFeatures.flags.test(MeshFeatures::IsSkybox),
    "Pipeline doesn't support skybox meshes");

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

  m_vertexAttributeDescriptions = createAttributeDescriptions(spec.meshFeatures.vertexLayout);

  m_vertexBindingDescriptions = {
    VkVertexInputBindingDescription{
      .binding = 0,
      .stride = static_cast<uint32_t>(vertexSize),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    }
  };

  m_vertexInputStateInfo = VkPipelineVertexInputStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindingDescriptions.size()),
    .pVertexBindingDescriptions = m_vertexBindingDescriptions.data(),
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributeDescriptions.size()),
    .pVertexAttributeDescriptions = m_vertexAttributeDescriptions.data()
  };

  m_inputAssemblyStateInfo = VkPipelineInputAssemblyStateCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };

  m_rasterizationStateInfo = defaultRasterizationState(false);
  m_multisampleStateInfo = defaultMultisamplingState();
  m_colourBlendStateInfo = defaultColourBlendState(m_colourBlendAttachmentState);
  m_depthStencilStateInfo = disabledDepthStencilState();

  m_descriptorSetLayouts = {
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Global),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::RenderPass),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Material),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Object)
  };

  m_pushConstantRanges = createPushConstantRanges(spec);

  m_layoutInfo = VkPipelineLayoutCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size()),
    .pSetLayouts = m_descriptorSetLayouts.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size()),
    .pPushConstantRanges = m_pushConstantRanges.size() == 0 ? nullptr : m_pushConstantRanges.data()
  };

  constructGraphicsPipeline(swapchainExtent);
}

void GfxOverlayPipeline::constructGraphicsPipeline(VkExtent2D swapchainExtent)
{
  destroyGraphicsPipeline();

  m_swapchainExtent = swapchainExtent;

  VK_CHECK(vkCreatePipelineLayout(m_device, &m_layoutInfo, nullptr, &m_layout),
    "Failed to create default pipeline layout");

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
    .pNext = nullptr,
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
    .renderPass = m_renderPass,
    .subpass = m_subpass,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1
  };

  VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
    &m_pipeline), "Failed to create default pipeline");

  m_logger.info(STR("GraphicsPipeline: " << m_pipeline));
}

void GfxOverlayPipeline::onViewportResize(VkExtent2D swapchainExtent)
{
  constructGraphicsPipeline(swapchainExtent);
}

void GfxOverlayPipeline::drawSprites(VkCommandBuffer commandBuffer, const SpriteNode& node,
  const Mat4x4f& transform)
{
  SpritePushConstants constants{
    .modelMatrix = node.modelMatrix * transform,
    .spriteUvCoords = {
      node.uvCoords[0],
      node.uvCoords[1],
      node.uvCoords[2],
      node.uvCoords[3]
    },
    .colour = node.colour
  };

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
    SPRITE_PUSH_CONSTANTS_VERT_OFFSET, SPRITE_PUSH_CONSTANTS_VERT_SIZE, &constants);

  void* fragPart = reinterpret_cast<char*>(&constants) + SPRITE_PUSH_CONSTANTS_FRAG_OFFSET;

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
    SPRITE_PUSH_CONSTANTS_FRAG_OFFSET, SPRITE_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
}

void GfxOverlayPipeline::drawQuads(VkCommandBuffer commandBuffer, const QuadNode& node,
  const Mat4x4f& transform)
{
  QuadPushConstants constants{
    .modelMatrix = node.modelMatrix * transform,
    .colour = node.colour,
    .radius = node.radius
  };

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
    QUAD_PUSH_CONSTANTS_VERT_OFFSET, QUAD_PUSH_CONSTANTS_VERT_SIZE, &constants);

  void* fragPart = reinterpret_cast<char*>(&constants) + QUAD_PUSH_CONSTANTS_FRAG_OFFSET;

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
    QUAD_PUSH_CONSTANTS_FRAG_OFFSET, QUAD_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
}

void GfxOverlayPipeline::drawDynamicText(VkCommandBuffer commandBuffer, const DynamicTextNode& node,
  const Mat4x4f& transform)
{
  DynamicTextPushConstants constants;
  constants.modelMatrix = node.modelMatrix * transform;
  constants.colour = node.colour;

  memset(constants.text, '\0', sizeof(constants.text));
  strncpy(reinterpret_cast<char*>(constants.text), node.text.c_str(), node.text.size());

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
    DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_OFFSET, DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_SIZE, &constants);

  void* fragPart = reinterpret_cast<char*>(&constants) + DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET;

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
    DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET, DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
}

// Probably static text
void GfxOverlayPipeline::drawDefault(VkCommandBuffer commandBuffer, const DefaultModelNode& node,
  const Mat4x4f& transform)
{
  if (node.type == RenderNodeType::DefaultModel) {
    assert(!node.meshFeatures.flags.test(MeshFeatures::IsInstanced));
    assert(!node.meshFeatures.flags.test(MeshFeatures::IsSkybox));

    auto& defaultNode = dynamic_cast<const DefaultModelNode&>(node);

    DefaultPushConstants constants{
      .modelMatrix = defaultNode.modelMatrix * transform,
      .shadowCascade = 0,
      ._padding0{},
      .colour = defaultNode.colour,
      .tick = 0
    };

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
      DEFAULT_PUSH_CONSTANTS_VERT_OFFSET, DEFAULT_PUSH_CONSTANTS_VERT_SIZE, &constants);

    void* fragPart = reinterpret_cast<char*>(&constants) + DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET;

    vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
      DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET, DEFAULT_PUSH_CONSTANTS_FRAG_SIZE, fragPart);
  }
}

std::vector<VkDescriptorSet> GfxOverlayPipeline::getDescriptorSets(size_t currentFrame,
  ResourceId meshId, ResourceId materialId) const
{
  auto globalDescriptorSet = m_renderResources.getGlobalDescriptorSet(currentFrame);
  auto renderPassDescriptorSet = m_renderResources.getRenderPassDescriptorSet(m_spec.renderPass,
    currentFrame);
  auto materialDescriptorSet = m_renderResources.getMaterialDescriptorSet(materialId);
  auto objectDescriptorSet = m_renderResources.getObjectDescriptorSet(meshId, currentFrame);

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

  return descriptorSets;
}

void GfxOverlayPipeline::recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
  BindState& bindState, size_t currentFrame, uint32_t, uint32_t)
{
  DBG_TRACE(m_logger);

  auto buffers = m_renderResources.getMeshBuffers(node.mesh, currentFrame);

  if (m_pipeline != bindState.pipeline) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  }

  VkBuffer vertexBuffers[] = { buffers.vertexBuffer };
  VkDeviceSize offsets[] = { 0 };

  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

  auto descriptorSets = getDescriptorSets(currentFrame, node.mesh, node.material);

  if (descriptorSets != bindState.descriptorSets) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0,
      static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
  }

  switch (node.type) {
    case RenderNodeType::Sprite: {
      drawSprites(commandBuffer, dynamic_cast<const SpriteNode&>(node), buffers.transform);
      break;
    }
    case RenderNodeType::Quad: {
      drawQuads(commandBuffer, dynamic_cast<const QuadNode&>(node), buffers.transform);
      break;
    }
    case RenderNodeType::DynamicText: {
      drawDynamicText(commandBuffer, dynamic_cast<const DynamicTextNode&>(node), buffers.transform);
      break;
    }
    default: {
      drawDefault(commandBuffer, dynamic_cast<const DefaultModelNode&>(node), buffers.transform);
      break;
    }
  }

  vkCmdDrawIndexed(commandBuffer, buffers.numIndices, 1, 0, 0, 0);

  bindState.pipeline = m_pipeline;
  bindState.descriptorSets = descriptorSets;
}

void GfxOverlayPipeline::destroyGraphicsPipeline()
{
  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
  }
  if (m_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  }
}

GfxOverlayPipeline::~GfxOverlayPipeline()
{
  if (m_vertShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device, m_vertShaderModule, nullptr);
  }
  if (m_fragShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_device, m_fragShaderModule, nullptr);
  }
  destroyGraphicsPipeline();
}

} // namespace

GraphicsPipelinePtr createGfxOverlayPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shaderProgram, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins)
{
  return std::make_unique<GfxOverlayPipeline>(spec, shaderProgram, renderResources, logger, device,
    renderPass, subpass, swapchainExtent, margins);
}

} // namespace render
} // namespace lithic3d
