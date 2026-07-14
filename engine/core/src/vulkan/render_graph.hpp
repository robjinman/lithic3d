#pragma once

#include "vulkan/render_resources.hpp"
#include <optional>
#include <map>

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

using RenderNodePtr = std::unique_ptr<RenderNode>;

class RenderGraph
{
  public:
    using KeyPart = long;
    using Key = std::array<KeyPart, 6>; // TODO: Try to reduce key size

    struct Entry
    {
      Key key;
      RenderNodePtr node;
    };

    RenderGraph()
    {
      m_entries.reserve(100);
    }

    inline void insert(const Key& key, RenderNodePtr value);
    inline void remove(const Key& key);

    class Iterator
    {
      public:
        explicit Iterator(std::vector<Entry>::const_iterator i)
          : m_i(i) {}

        bool operator==(const Iterator& rhs) const
        {
          return m_i == rhs.m_i;
        }

        const RenderNodePtr& operator*() const
        {
          return m_i->node;
        }

        const RenderNodePtr* operator->() const
        {
          return &(**this);
        }

        void next()
        {
          ++m_i;
        }

        Iterator& operator++()
        {
          next();
          return *this;
        }

      private:
        std::vector<Entry>::const_iterator m_i;
    };

    inline Iterator begin() const;
    inline Iterator end() const;

    inline size_t size() const;

  private:
    std::vector<Entry> m_entries;
};

inline void RenderGraph::insert(const Key& key, RenderNodePtr value)
{
  int j = static_cast<int>(m_entries.size()) - 1;
  m_entries.resize(m_entries.size() + 1);

  while (j >= 0 && key < m_entries[j].key) {
    m_entries[j + 1] = std::move(m_entries[j]);
    --j;
  }

  m_entries[j + 1] = { key, std::move(value) };
}

inline void RenderGraph::remove(const Key&)
{
  // TODO
  EXCEPTION("Not implemented");
}

inline RenderGraph::Iterator RenderGraph::begin() const
{
  return Iterator{m_entries.cbegin()};
}

inline RenderGraph::Iterator RenderGraph::end() const
{
  return Iterator{m_entries.cend()};
}

inline size_t RenderGraph::size() const
{
  return m_entries.size();
}

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
