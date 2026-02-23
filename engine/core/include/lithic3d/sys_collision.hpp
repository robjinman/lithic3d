#pragma once

#include "sys_spatial.hpp"
#include "renderables.hpp"
#include "utils.hpp"

namespace lithic3d
{

const size_t MAX_FORCES = 8;

struct Force
{
  Vec3f force;
  uint32_t lifetime = 0;
};

struct CCollision
{
  float inverseMass = 1.f;
  //Vec3f centreOfMass;
  //float damping = 0.f;
  std::array<Force, MAX_FORCES> forces;
  Vec3f acceleration;
  Vec3f velocity;

  static constexpr ComponentTypeId TypeId = CCollisionTypeId;
};

struct HeightMap
{
  float width = 0.f;  // World units
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

//namespace CollisionFlags
//{
//  enum : size_t
//  {
//  };
//}

struct DCollision
{
  using RequiredComponents = type_list<CSpatialFlags, CBoundingBox, CLocalTransform, CCollision>;

  float inverseMass = 1.f;
  //std::bitset<16> flags;
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
    virtual void applyForce(EntityId id, const Vec3f& force, float seconds) = 0;
    virtual void setInverseMass(EntityId id, float inverseMass) = 0;
    virtual void setStationary(EntityId id) = 0;

    virtual void addEntity(EntityId id, const DCollision& data) = 0;
    virtual void addEntity(EntityId id, const DTerrainChunk data) = 0;

    static const SystemId id = Systems::Collision;
};

using SysCollisionPtr = std::unique_ptr<SysCollision>;

class EventSystem;

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

}
