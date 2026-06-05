#include "vulkan/pipelines/compute_pipeline.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "vulkan/render_resources.hpp"
#include "lithic3d/vulkan/shader.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include <array>

namespace lithic3d
{
namespace render
{
namespace
{

// Remember to update offsets in shader after updating these
#pragma pack(push, 4)
struct PushConstants
{
  uint32_t tick;      // 4 bytes
  //uint8_t _pad0[12];  // 12 bytes
};
#pragma pack(pop)

class CmpParticlesPipeline : public ComputePipeline
{
  public:
    CmpParticlesPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader, Logger& logger,
      VkDevice device, const RenderResources& renderResources);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame,
      uint32_t tick) override;

    ~CmpParticlesPipeline() override;

  private:
    Logger& m_logger;
    VkDevice m_device;
    const RenderResources& m_renderResources;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    VkPipelineLayout createPipelineLayout();
    VkShaderModule createShaderModule(const ShaderByteCode& shaderCode);
};

CmpParticlesPipeline::CmpParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader, Logger& logger, VkDevice device,
  const RenderResources& renderResources)
  : m_logger(logger)
  , m_device(device)
  , m_renderResources(renderResources)
{
  m_pipelineLayout = createPipelineLayout();

  auto shaderModule = createShaderModule(shader.computeShaderCode);

  VkPipelineShaderStageCreateInfo computeShaderStageInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = shaderModule,
    .pName = "main",
    .pSpecializationInfo = nullptr
  };

  VkComputePipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = computeShaderStageInfo,
    .layout = m_pipelineLayout,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = 0
  };

  VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
    &m_pipeline), "Failed to create compute pipeline");

  vkDestroyShaderModule(m_device, shaderModule, nullptr);

  m_logger.info(STR("Pipeline: " << m_pipeline));
}

VkShaderModule CmpParticlesPipeline::createShaderModule(const ShaderByteCode& shaderCode)
{
  VkShaderModuleCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .codeSize = shaderCode.size() * sizeof(uint32_t),
    .pCode = shaderCode.data()
  };

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule),
    "Failed to create shader module");

  return shaderModule;
}

VkPipelineLayout CmpParticlesPipeline::createPipelineLayout()
{
  VkPushConstantRange pushConstantRange{
    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    .offset = 0,
    .size = sizeof(PushConstants)
  };

  auto buffers = m_renderResources.getParticleBuffers();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = 1,
    .pSetLayouts = &buffers.descriptorSetLayout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pushConstantRange
  };

  VkPipelineLayout pipelineLayout;

  VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
    "Failed to create pipeline layout");

  return pipelineLayout;
}

void CmpParticlesPipeline::recordCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame,
  uint32_t tick)
{
  auto buffers = m_renderResources.getParticleBuffers();

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
    &buffers.descriptorSets[currentFrame], 0, 0);

  PushConstants pushConstants{
    .tick = tick
  };

  vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
    sizeof(PushConstants), &pushConstants);

  vkCmdDispatch(commandBuffer, 256 / PARTICLE_COUNT, 1, 1);
}

CmpParticlesPipeline::~CmpParticlesPipeline()
{
  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
  }
  if (m_pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  }
}

} // namespace

ComputePipelinePtr createCmpParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader, Logger& logger, VkDevice device,
  const RenderResources& renderResources)
{
  return std::make_unique<CmpParticlesPipeline>(spec, shader, logger, device, renderResources);
}

} // namespace render
} // namespace lithic3d
