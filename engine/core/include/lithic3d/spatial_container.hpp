#pragma once

#include "component_store.hpp"
#include "math.hpp"
#include <memory>
#include <set>

namespace lithic3d
{

struct Plane
{
  Vec3f normal;
  float distance;
};

using Frustum = std::array<Plane, 6>;

class SpatialContainer
{
  public:
    virtual void insert(EntityId entityId, const Vec3f& pos, float diameter) = 0;
    virtual void move(EntityId entityId, const Vec3f& pos, float diameter) = 0;
    virtual void remove(EntityId entityId) = 0;
    virtual std::set<EntityId> getIntersecting(const Frustum& volume) const = 0;

    virtual ~SpatialContainer() = default;
};

using SpatialContainerPtr = std::unique_ptr<SpatialContainer>;

SpatialContainerPtr createSpatialContainer();

}
