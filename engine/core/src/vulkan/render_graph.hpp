#pragma once

#include "vulkan/render_resources.hpp"
#include "lithic3d/tree_set.hpp"
#include <optional>

namespace lithic3d
{
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
  DynamicText,
  Particles
};

struct RenderNode
{
  RenderNode(RenderNodeType type)
    : type(type) {}

  RenderNodeType type;
  ResourceId mesh = NULL_RESOURCE_ID;
  ResourceId material = NULL_RESOURCE_ID;
  MeshFeatureSet meshFeatures{};
  MaterialFeatureSet materialFeatures{};
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

struct ParticlesNode : public RenderNode
{
  ParticlesNode()
    : RenderNode(RenderNodeType::Particles)
  {}

  Mat4x4f modelMatrix;
};

struct SkyboxNode : public RenderNode
{
  SkyboxNode()
    : RenderNode(RenderNodeType::Skybox)
  {}
};

} // namespace render
} // namespace lithic3d
