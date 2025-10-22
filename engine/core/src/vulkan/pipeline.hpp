#pragma once

#include "tree_set.hpp"
#include "shader_system.hpp"
#include "vulkan/render_resources.hpp"
#include <vulkan/vulkan.h>
#include <optional>

class Logger;

namespace render
{

enum class RenderNodeType
{
  DefaultModel,
  InstancedModel,
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
  MeshHandle mesh;
  MaterialHandle material;
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

    virtual void recordCommandBuffer(VkCommandBuffer commandBuffer, const RenderNode& node,
      BindState& bindState, size_t currentFrame) = 0;

    virtual ~Pipeline() {}
};

using PipelinePtr = std::unique_ptr<Pipeline>;

PipelinePtr createPipeline(const ShaderProgramSpec& spec, const ShaderProgram& shader,
  const RenderResources& renderResources, Logger& logger, VkDevice device,
  VkExtent2D swapchainExtent, VkFormat swapchainImageFormat, VkFormat depthFormat,
  const ScreenMargins& margins);

} // namespace render

template<>
struct std::hash<Recti>
{
  std::size_t operator()(const Recti& rect) const noexcept
  {
    return hashAll(rect.x, rect.y, rect.w, rect.h);
  }
};
