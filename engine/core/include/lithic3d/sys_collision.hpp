#pragma once

#include "sys_spatial.hpp"
#include "renderables.hpp"
#include "utils.hpp"

namespace lithic3d
{

struct CSphere
{
  float radius = 0.f;

  static constexpr ComponentType TypeId = CSphereTypeId;
};

struct DSphere
{
  using RequiredComponents = type_list<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox, CSphere
  >;

  float radius = 0.f;
};

struct HeightMap
{
  float width = 0.f;
  float height = 0.f;
  size_t widthPx = 0.f;
  size_t heightPx = 0.f;
  std::vector<float> data;

  inline float at(float x, float y) const
  {
    DBG_ASSERT(inRange(x, 0.f, width), "Value out of range");
    DBG_ASSERT(inRange(y, 0.f, height), "Value out of range");

    float xPx = x * widthPx / width;
    float yPx = y * heightPx / height;

    float height = data.at(yPx * widthPx + xPx);

    // TODO: Interpolation. We need to interpolate between the heights of the 3 vertices of the
    // triangle that (x, y) is part of. This requires knowing how the mesh is triangulated.

    return height;
  }
};

struct DTerrainChunk
{
  using RequiredComponents = type_list<
    CSpatialFlags, CGlobalTransform, CBoundingBox
  >;

  HeightMap heightMap;
};

class SysCollision : public System
{
  public:
    virtual void addEntity(EntityId id, const DSphere& data) = 0;
    virtual void addEntity(EntityId id, const DTerrainChunk data) = 0;

    static const SystemId id = COLLISION_SYSTEM;
};

using SysCollisionPtr = std::unique_ptr<SysCollision>;

class EventSystem;

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

}
