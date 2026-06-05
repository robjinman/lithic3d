#pragma once

#include <vulkan/vulkan.h>
#include <memory>

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

} // namespace render
} // namespace lithic3d
