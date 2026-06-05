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

class GfxParticlesPipeline : public GraphicsPipeline
{
  public:
    GfxParticlesPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
      const RenderResources& renderResources, Logger& logger, VkDevice device,
      VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
      const ScreenMargins& margins);

    void onViewportResize(VkExtent2D swapchainExtent) override;

    void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame, uint32_t tick, uint32_t shadowMapCascade) override;

    ~GfxParticlesPipeline() override;

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

    void constructGraphicsPipeline(VkExtent2D swapchainExtent);
    void destroyGraphicsPipeline();
};

GfxParticlesPipeline::GfxParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins)
  : m_logger(logger)
  , m_renderResources(renderResources)
  , m_spec(spec)
  , m_device(device)
  , m_renderPass(renderPass)
  , m_subpass(subpass)
  , m_margins(margins)
{
  ASSERT(spec.renderPass == RenderPass::Main, "Pipeline doesn't support render pass");

  ASSERT(spec.meshFeatures.flags.test(MeshFeatures::IsParticles),
    "Pipeline doesn't support mesh type");

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
  
  m_vertexAttributeDescriptions = {
    VkVertexInputAttributeDescription{
      .location = 0,
      .binding = 0,
      .format = attributeFormat(BufferUsage::AttrPosition),
      .offset = static_cast<uint32_t>(calcOffsetInVertex(spec.meshFeatures.vertexLayout,
        BufferUsage::AttrPosition))
    },
    VkVertexInputAttributeDescription{
      .location = 1,
      .binding = 0,
      .format = attributeFormat(BufferUsage::AttrTexCoord),
      .offset = static_cast<uint32_t>(calcOffsetInVertex(spec.meshFeatures.vertexLayout,
        BufferUsage::AttrTexCoord))
    },
    VkVertexInputAttributeDescription{
      .location = 2,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = offsetof(Particle, position)
    },
    VkVertexInputAttributeDescription{
      .location = 3,
      .binding = 1,
      .format = VK_FORMAT_R32_SFLOAT,
      .offset = offsetof(Particle, size)
    },
    VkVertexInputAttributeDescription{
      .location = 4,
      .binding = 1,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .offset = offsetof(Particle, colour)
    }
  };

  size_t vertexSize = 0;
  for (auto usage : spec.meshFeatures.vertexLayout) {
    vertexSize += getAttributeSize(usage);
  }

  m_vertexBindingDescriptions = {
    VkVertexInputBindingDescription{
      .binding = 0,
      .stride = static_cast<uint32_t>(vertexSize),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    },
    VkVertexInputBindingDescription{
      .binding = 1,
      .stride = sizeof(Particle),
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
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

  m_rasterizationStateInfo = defaultRasterizationState(true);

  m_multisampleStateInfo = defaultMultisamplingState();
  m_colourBlendStateInfo = defaultColourBlendState(m_colourBlendAttachmentState);
  m_depthStencilStateInfo = defaultDepthStencilState();
  m_depthStencilStateInfo.depthWriteEnable = VK_FALSE;

  m_descriptorSetLayouts = {
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Global),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::RenderPass),
    m_renderResources.getDescriptorSetLayout(DescriptorSetNumber::Material)
  };

  m_pushConstantRanges = {
    VkPushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = PARTICLES_PUSH_CONSTANTS_VERT_OFFSET,
      .size = PARTICLES_PUSH_CONSTANTS_VERT_SIZE
    }
  };

  m_layoutInfo = VkPipelineLayoutCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size()),
    .pSetLayouts = m_descriptorSetLayouts.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size()),
    .pPushConstantRanges = m_pushConstantRanges.data()
  };

  constructGraphicsPipeline(swapchainExtent);
}

void GfxParticlesPipeline::constructGraphicsPipeline(VkExtent2D swapchainExtent)
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

void GfxParticlesPipeline::onViewportResize(VkExtent2D swapchainExtent)
{
  constructGraphicsPipeline(swapchainExtent);
}

void GfxParticlesPipeline::recordCommandBuffer(VkCommandBuffer commandBuffer,
  const RenderNode& node, BindState& bindState, size_t currentFrame, uint32_t, uint32_t)
{
  DBG_TRACE(m_logger);

  auto globalDescriptorSet = m_renderResources.getGlobalDescriptorSet(currentFrame);
  auto renderPassDescriptorSet = m_renderResources.getRenderPassDescriptorSet(m_spec.renderPass,
    currentFrame);
  auto materialDescriptorSet = m_renderResources.getMaterialDescriptorSet(node.material);
  auto objectDescriptorSet = m_renderResources.getObjectDescriptorSet(node.mesh, currentFrame);

  if (m_pipeline != bindState.pipeline) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  }

  std::vector<VkDescriptorSet> descriptorSets{
    globalDescriptorSet,
    renderPassDescriptorSet,
    materialDescriptorSet
  };

  auto buffers = m_renderResources.getParticleBuffers();

  std::vector<VkBuffer> vertexBuffers{
    buffers.vertexBuffer,
    buffers.ssbos[currentFrame]
  };

  std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
  vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(vertexBuffers.size()),
    vertexBuffers.data(), offsets.data());
  vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

  if (descriptorSets != bindState.descriptorSets) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0,
      static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
  }

  auto& particlesNode = dynamic_cast<const ParticlesNode&>(node);

  ParticlesPushConstants constants{
    .modelMatrix = particlesNode.modelMatrix
  };

  vkCmdPushConstants(commandBuffer, m_layout, VK_SHADER_STAGE_VERTEX_BIT,
    PARTICLES_PUSH_CONSTANTS_VERT_OFFSET, PARTICLES_PUSH_CONSTANTS_VERT_SIZE, &constants);

  vkCmdDrawIndexed(commandBuffer, buffers.numIndices, PARTICLE_COUNT, 0, 0, 0);

  bindState.pipeline = m_pipeline;
  bindState.descriptorSets = descriptorSets;
}

void GfxParticlesPipeline::destroyGraphicsPipeline()
{
  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
  }
  if (m_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  }
}

GfxParticlesPipeline::~GfxParticlesPipeline()
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

GraphicsPipelinePtr createGfxParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shaderProgram, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins)
{
  return std::make_unique<GfxParticlesPipeline>(spec, shaderProgram, renderResources, logger,
    device, renderPass, subpass, swapchainExtent, margins);
}

} // namespace render
} // namespace lithic3d
