#pragma once

#include "component_store.hpp"
#include "math.hpp"
#include <memory>
#include <set>
#include <array>

namespace lithic3d
{

struct OctreeCell
{
  Vec3f min;
  float size = 0.f;
};

struct OctreeNode
{
  OctreeNode* parent = nullptr;
  uint8_t index = 0; // 0 to 7
  OctreeCell tightBounds;
  OctreeCell looseBounds;
  uint8_t numChildren = 0;
  std::array<std::unique_ptr<OctreeNode>, 8> children;
  std::set<EntityId> objects;
};

class LooseOctree
{
  public:
    virtual void insert(EntityId entityId, const Vec3f& pos, float radius) = 0;
    virtual void move(EntityId entityId, const Vec3f& pos, float radius) = 0;
    virtual void remove(EntityId entityId) = 0;
    virtual std::set<EntityId> getIntersecting(const Frustum& volume) const = 0;

    virtual ~LooseOctree() = default;

    // Exposed for testing
    virtual const OctreeNode& test_root() const = 0;
};

using LooseOctreePtr = std::unique_ptr<LooseOctree>;

LooseOctreePtr createLooseOctree(const Vec3f& min, float size);

}
