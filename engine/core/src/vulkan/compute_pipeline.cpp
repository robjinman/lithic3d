#include "vulkan/compute_pipeline.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "vulkan/render_resources.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include <array>
#include <random> // TODO: Remove

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
};
#pragma pack(pop)

class ComputePipelineImpl : public ComputePipeline
{
  public:
    ComputePipelineImpl(const ShaderProgramSpec& spec, const ShaderProgram& shader, Logger& logger,
      VkDevice device, RenderResources& renderResources);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame) override;

    ~ComputePipelineImpl() override;

  private:
    Logger& m_logger;
    VkDevice m_device;
    RenderResources& m_renderResources;
    //GpuBufferManager& m_gpuBufferManager;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // TODO: Move these into render resources?
    //VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    //VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    //std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_descriptorSets;
    //std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_ubos;
    //std::array<GpuBufferPtr, 2> m_ssbos;

    //void createDescriptorSetLayout();
    VkPipelineLayout createPipelineLayout();
    VkShaderModule createShaderModule(const ShaderByteCode& shaderCode);
    //void createBuffers();
    //void createDescriptorPool();
    //void createDescriptorSets();
};

ComputePipelineImpl::ComputePipelineImpl(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  Logger& logger, VkDevice device, RenderResources& renderResources)
  : m_logger(logger)
  , m_device(device)
  , m_renderResources(renderResources)
{
  //createDescriptorSetLayout();
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

  //createBuffers();
  //createDescriptorPool();
  //createDescriptorSets();
}

// TODO: Use the pool from RenderResources
/*
void ComputePipelineImpl::createDescriptorPool()
{
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[0].descriptorCount = 128; // TODO

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[1].descriptorCount = 32; // TODO

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 32; // TODO

  VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool),
    "Failed to create descriptor pool");
}*/

VkShaderModule ComputePipelineImpl::createShaderModule(const ShaderByteCode& shaderCode)
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

VkPipelineLayout ComputePipelineImpl::createPipelineLayout()
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

void ComputePipelineImpl::recordCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame)
{
  auto buffers = m_renderResources.getParticleBuffers();

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
    &buffers.descriptorSets[currentFrame], 0, 0);

  static uint32_t tick = 0;
  PushConstants pushConstants{
    .tick = tick++
  };

  vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
    sizeof(PushConstants), &pushConstants);

  vkCmdDispatch(commandBuffer, 256 / PARTICLE_COUNT, 1, 1);
}

ComputePipelineImpl::~ComputePipelineImpl()
{
  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
  }
  if (m_pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  }
}

} // namespace

ComputePipelinePtr createComputePipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  Logger& logger, VkDevice device, RenderResources& renderResources)
{
  return std::make_unique<ComputePipelineImpl>(spec, shader, logger, device, renderResources);
}

} // namespace render
} // namespace lithic3d
