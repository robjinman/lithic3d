// The collision system ignores an object's global transform and reads/updates only its local
// transform. Essentially, objects are treated as though they are unparented.

// TODO: Remove centre of mass?

#pragma once

#include "sys_spatial.hpp"
#include "renderables.hpp"
#include "utils.hpp"

namespace lithic3d
{

const size_t MAX_FORCES = 4;

struct Force
{
  Vec3f force;
  uint32_t lifetime = 0;
};

// In model space
struct BoundingBox
{
  Vec3f min;
  Vec3f max;
  Mat4x4f transform;
};

struct CCollision
{
  float restitution = 0.2f;
  float friction = 0.4f;
  bool isPolyhedron = false;

  static constexpr ComponentTypeId TypeId = CCollisionTypeId;
};

struct Capsule
{
  // TODO
};

struct HeightMap
{
  float width = 0.f;  // World units
  float height = 0.f;
  uint32_t widthPx = 0.f;
  uint32_t heightPx = 0.f;
  std::vector<float> data;
};

struct Edge
{
  Vec3f A;
  Vec3f B;
};

class HeightMapSampler
{
  public:
    HeightMapSampler(const HeightMap& heightMap, const Vec3f& position)
      : m_map(heightMap)
      , m_pos(position) {}

    std::array<Vec3f, 3> triangle(const Vec2f& p) const;
    void vertices(const Vec2f& min, const Vec2f& max, std::vector<Vec3f>& vertices) const;
    void edges(const Vec2f& min, const Vec2f& max, std::vector<Edge>& edges) const;

  private:
    const HeightMap& m_map;
    Vec3f m_pos;
};

struct CCollisionTerrain
{
  HeightMap heightMap;

  static constexpr ComponentTypeId TypeId = CCollisionTerrainTypeId;
};

struct CCollisionBox
{
  BoundingBox boundingBox;

  static constexpr ComponentTypeId TypeId = CCollisionBoxTypeId;
};

struct CCollisionCapsule
{
  Capsule capsule;

  static constexpr ComponentTypeId TypeId = CCollisionCapsuleTypeId;
};

struct CCollisionDynamic
{
  float inverseMass = 1.f;
  Vec3f centreOfMass;
  std::array<Force, MAX_FORCES> linearForces;
  Vec3f linearAcceleration;
  Vec3f linearVelocity;
  std::array<Force, MAX_FORCES> torques;
  Vec3f angularAcceleration;
  Vec3f angularVelocity;
  Mat3x3f inverseInertialTensor;
  Vec3f resolverDeltaLinearV;
  Vec3f resolverDeltaAngularV;
  uint16_t resolverNumAdjustments = 0;
  uint16_t framesIdle = 0;
  bool idle = false;
  bool hasCollided = false;

  static constexpr ComponentTypeId TypeId = CCollisionDynamicTypeId;
};

struct DDynamicBox
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionDynamic, CCollisionBox
  >;

  float inverseMass = 1.f;
  float restitution = 0.2f;
  float friction = 0.4f;
  Vec3f centreOfMass;
  BoundingBox boundingBox;
};

struct DStaticBox
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionBox
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  BoundingBox boundingBox;
};

struct DTerrainChunk
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionTerrain
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  HeightMap heightMap;
};

// TODO: Support poly aggregates
struct DPolyhedron
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  Vec3f centreOfMass;
  std::vector<Vec3f> vertices;
  std::vector<uint16_t> indices;
};

struct DCapsule
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionDynamic, CCollisionCapsule
  >;

  float inverseMass = 1.f;
  float restitution = 0.2f;
  float friction = 0.4f;
  Vec3f centreOfMass;
  Capsule capsule;
};

class SysCollision : public System
{
  public:
    virtual void setInverseMass(EntityId id, float inverseMass) = 0;
    // TODO: function to apply force at offset
    virtual void applyForce(EntityId id, const Vec3f& force, float seconds) = 0;
    virtual void applyTorque(EntityId id, const Vec3f& torque, float seconds) = 0;
    virtual void setStationary(EntityId id) = 0;

    virtual void addEntity(EntityId id, const DStaticBox& data) = 0;
    virtual void addEntity(EntityId id, const DDynamicBox& data) = 0;
    virtual void addEntity(EntityId id, const DTerrainChunk& data) = 0;
    virtual void addEntity(EntityId id, const DPolyhedron& data) = 0;
    virtual void addEntity(EntityId id, const DCapsule& data) = 0;

    static const SystemId id = Systems::Collision;
};

using SysCollisionPtr = std::unique_ptr<SysCollision>;

class EventSystem;

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

}
