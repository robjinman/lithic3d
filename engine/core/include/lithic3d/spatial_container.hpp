#pragma once

#include "component_store.hpp"
#include "math.hpp"
#include <memory>

namespace lithic3d
{

struct Plane
{
  Vec3f normal;
  float distance;
};

struct Aabb
{
  Vec3f min;
  Vec3f max;
};

using Frustum = std::array<Plane, 6>;

class SpatialContainer
{
  public:
    virtual void insert(EntityId entityId, const Aabb& aabb) = 0;
    virtual void move(EntityId entityId, const Aabb& aabb) = 0;
    virtual void remove(EntityId entityId) = 0;
    virtual void getIntersecting(const Frustum& volume, std::vector<EntityId>& entities) const = 0;

    virtual ~SpatialContainer() = default;
};

using SpatialContainerPtr = std::unique_ptr<SpatialContainer>;

SpatialContainerPtr createSpatialContainer();

}
