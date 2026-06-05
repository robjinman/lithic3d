#pragma once


#include "lithic3d/vulkan/shader.hpp"
#include "vulkan/render_graph.hpp"
#include <vulkan/vulkan.h>

namespace lithic3d
{

class Logger;

namespace render
{

struct BindState
{
  VkPipeline pipeline;
  std::vector<VkDescriptorSet> descriptorSets;
};

class GraphicsPipeline
{
  public:
    virtual void onViewportResize(VkExtent2D swapchainExtent) = 0;

    // TODO: Remove shadowMapCascade - find another way to pass this info
    virtual void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame, uint32_t tick, uint32_t shadowMapCascade) = 0;

    virtual ~GraphicsPipeline() {}
};

using GraphicsPipelinePtr = std::unique_ptr<GraphicsPipeline>;

} // namespace render
} // namespace lithic3d
