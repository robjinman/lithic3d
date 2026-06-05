#include "vulkan/pipelines/pipeline_factory.hpp"

namespace lithic3d
{
namespace render
{

GraphicsPipelinePtr createGfxDefaultPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shaderProgram, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins);

GraphicsPipelinePtr createGfxOverlayPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shaderProgram, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins);

GraphicsPipelinePtr createGfxParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shaderProgram, const RenderResources& renderResources, Logger& logger,
  VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkExtent2D swapchainExtent,
  const ScreenMargins& margins);

ComputePipelinePtr createCmpParticlesPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader, Logger& logger, VkDevice device,
  const RenderResources& renderResources);

namespace
{

class PipelineFactoryImpl : public PipelineFactory
{
  public: 
    PipelineFactoryImpl(const RenderResources& renderResources, Logger& logger, VkDevice device);

    GraphicsPipelinePtr createGraphicsPipeline(const ShaderProgramSpec& spec,
      const ShaderProgram& shader, VkRenderPass renderPass, uint32_t subpass,
      VkExtent2D swapchainExtent, const ScreenMargins& margins) const override;

    ComputePipelinePtr createComputePipeline(const ShaderProgramSpec& spec,
      const ShaderProgram& shader) override;

  private:
    Logger& m_logger;
    const RenderResources& m_renderResources;
    VkDevice m_device;
};

PipelineFactoryImpl::PipelineFactoryImpl(const RenderResources& renderResources, Logger& logger,
  VkDevice device)
  : m_logger(logger)
  , m_renderResources(renderResources)
  , m_device(device)
{
}

GraphicsPipelinePtr PipelineFactoryImpl::createGraphicsPipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader, VkRenderPass renderPass, uint32_t subpass,
  VkExtent2D swapchainExtent, const ScreenMargins& margins) const
{
  if (spec.renderPass == RenderPass::Overlay) {
    return createGfxOverlayPipeline(spec, shader, m_renderResources, m_logger, m_device, renderPass,
      subpass, swapchainExtent, margins);
  }
  else if (spec.meshFeatures.flags.test(MeshFeatures::IsParticles)) {
    return createGfxParticlesPipeline(spec, shader, m_renderResources, m_logger, m_device,
      renderPass, subpass, swapchainExtent, margins);
  }
  else {
    return createGfxDefaultPipeline(spec, shader, m_renderResources, m_logger, m_device, renderPass,
      subpass, swapchainExtent, margins);
  }
}

ComputePipelinePtr PipelineFactoryImpl::createComputePipeline(const ShaderProgramSpec& spec,
  const ShaderProgram& shader)
{
  return createCmpParticlesPipeline(spec, shader, m_logger, m_device, m_renderResources);
}

} // namespace

PipelineFactoryPtr createPipelineFactory(const RenderResources& renderResources, Logger& logger,
  VkDevice device)
{
  return std::make_unique<PipelineFactoryImpl>(renderResources, logger, device);
}

} // namespace render
} // namespace lithic3d
