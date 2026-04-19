#pragma once

#include "lithic3d/tree_set.hpp"
#include "lithic3d/vulkan/shader.hpp"
#include "vulkan/render_resources.hpp"
#include <vulkan/vulkan.h>
#include <optional>

namespace lithic3d
{

class Logger;

namespace render
{

enum class RenderNodeType
{
  DefaultModel,
  InstancedModel,
  TerrainChunk,
  Skybox,
  Sprite,
  Quad,
  DynamicText
};

struct RenderNode
{
  RenderNode(RenderNodeType type)
    : type(type) {}

  RenderNodeType type;
  ResourceId mesh = NULL_RESOURCE_ID;
  ResourceId material = NULL_RESOURCE_ID;
  MeshFeatureSet meshFeatures;
  MaterialFeatureSet materialFeatures;
  uint32_t scissorId = 0;

  virtual ~RenderNode() = default;
};

using RenderGraphKey = long;
using RenderNodePtr = std::unique_ptr<RenderNode>;
using RenderGraph = TreeSet<RenderGraphKey, RenderNodePtr>; // TODO: Store by value?

// TODO: Replace inheritance

struct DefaultModelNode : public RenderNode
{
  DefaultModelNode()
    : RenderNode(RenderNodeType::DefaultModel)
  {}

  Mat4x4f modelMatrix;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
  std::optional<std::vector<Mat4x4f>> jointTransforms;
};

struct InstancedModelNode : public RenderNode
{
  InstancedModelNode()
    : RenderNode(RenderNodeType::InstancedModel)
  {}

  std::vector<MeshInstance> instances;
};

struct SpriteNode : public RenderNode
{
  SpriteNode()
    : RenderNode(RenderNodeType::Sprite)
  {}

  Mat4x4f modelMatrix;
  std::array<Vec2f, 4> uvCoords;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct QuadNode : public RenderNode
{
  QuadNode()
    : RenderNode(RenderNodeType::Quad)
  {}

  Mat4x4f modelMatrix;
  float radius = 0.f;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct DynamicTextNode : public RenderNode
{
  DynamicTextNode()
    : RenderNode(RenderNodeType::DynamicText)
  {}

  Mat4x4f modelMatrix;
  std::string text;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct SkyboxNode : public RenderNode
{
  SkyboxNode()
    : RenderNode(RenderNodeType::Skybox)
  {}
};

struct BindState
{
  VkPipeline pipeline;
  std::vector<VkDescriptorSet> descriptorSets;
};

class Pipeline
{
  public:
    virtual void onViewportResize(VkExtent2D swapchainExtent) = 0;

    // TODO: Remove shadowMapCascade - find another way to pass this info
    virtual void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame, uint32_t shadowMapCascade) = 0;

    virtual ~Pipeline() {}
};

using PipelinePtr = std::unique_ptr<Pipeline>;

PipelinePtr createPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  const RenderResources& renderResources, Logger& logger, VkDevice device, VkRenderPass renderPass,
  uint32_t subpass, VkExtent2D swapchainExtent, VkFormat swapchainImageFormat, VkFormat depthFormat,
  const ScreenMargins& margins);

} // namespace render
} // namespace lithic3d

template<>
struct std::hash<lithic3d::Recti>
{
  std::size_t operator()(const lithic3d::Recti& rect) const noexcept
  {
    return lithic3d::hashAll(rect.x, rect.y, rect.w, rect.h);
  }
};
