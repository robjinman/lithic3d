#pragma once

#include "vulkan/pipelines/graphics_pipeline.hpp"
#include "vulkan/pipelines/compute_pipeline.hpp"

namespace lithic3d
{
namespace render
{

class PipelineFactory
{
  public:
    virtual GraphicsPipelinePtr createGraphicsPipeline(const ShaderProgramSpec& spec,
      const ShaderProgram& shader, VkRenderPass renderPass, uint32_t subpass,
      VkExtent2D swapchainExtent, const ScreenMargins& margins) const = 0;

    virtual ComputePipelinePtr createComputePipeline(const ShaderProgramSpec& spec,
      const ShaderProgram& shader) = 0;

    virtual ~PipelineFactory() = default;
};

using PipelineFactoryPtr = std::unique_ptr<PipelineFactory>;

class RenderResources;

PipelineFactoryPtr createPipelineFactory(const RenderResources& renderResources, Logger& logger,
  VkDevice device, bool editorMode);

} // namespace render
} // namespace lithic3d
