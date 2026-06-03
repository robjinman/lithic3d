#pragma once

#include "lithic3d/vulkan/shader.hpp"
#include <vulkan/vulkan.h>

namespace lithic3d
{

class Logger;

namespace render
{

class ComputePipeline
{
  public:
    virtual void recordCommandBuffer(VkCommandBuffer commandBuffer, size_t currentFrame,
      uint32_t tick) = 0;

    virtual ~ComputePipeline() = default;
};

using ComputePipelinePtr = std::unique_ptr<ComputePipeline>;

class RenderResources;

ComputePipelinePtr createComputePipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  Logger& logger, VkDevice device, RenderResources& renderResources);

} // namespace render
} // namespace lithic3d
