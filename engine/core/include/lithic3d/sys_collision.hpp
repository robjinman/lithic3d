// TODO: We currently assume all dynamic objects are unparented - i.e. their parent is the spatial
// root node. Therefore we only read/write their local transforms.

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
  Mat4x4f transform = identityMatrix<4>();
};

struct CCollision
{
  float restitution = 0.2f;
  float friction = 0.4f;
  bool isPolyhedron = false;

  static constexpr ComponentTypeId TypeId = CCollisionTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollision>);

struct Capsule
{
  float radius = 0.f;
  float height = 0.f;
  Vec3f translation;  // TODO: Do we support off-centre capsules?
};

struct Ovoid
{
  float radius;
  Mat4x4f transform = identityMatrix<4>();
};

struct Cylinder
{
  float radius = 0.f;
  float height = 0.f;
  Mat4x4f transform = identityMatrix<4>();
};

// TODO
const size_t TERRAIN_CHUNK_NUM_VERTS = 16641;

struct HeightMap
{
  float width = 0.f;  // World units
  float height = 0.f;
  uint32_t widthPx = 0;
  uint32_t heightPx = 0;
  std::array<float, TERRAIN_CHUNK_NUM_VERTS> data;
};

struct Edge
{
  Vec3f A;
  Vec3f B;
};

using Triangle = std::array<Vec3f, 3>;

class HeightMapSampler
{
  public:
    HeightMapSampler(const HeightMap& heightMap, const Vec3f& position)
      : m_map(heightMap)
      , m_pos(position) {}

    inline bool inRange(const Vec2f& p) const;
    Triangle triangle(const Vec2f& p) const;
    void vertices(const Vec2f& min, const Vec2f& max, std::vector<Vec3f>& vertices) const;
    void edges(const Vec2f& min, const Vec2f& max, std::vector<Edge>& edges) const;
    void triangles(const Vec2f& min, const Vec2f& max,
      std::vector<Triangle>& triangles) const;

  private:
    const HeightMap& m_map;
    Vec3f m_pos;
};

inline bool HeightMapSampler::inRange(const Vec2f& p) const
{
  return lithic3d::inRange(p[0], m_pos[0], m_pos[0] + m_map.width) &&
    lithic3d::inRange(p[1], m_pos[2], m_pos[2] + m_map.height);
}

struct CCollisionTerrain
{
  HeightMap heightMap;

  static constexpr ComponentTypeId TypeId = CCollisionTerrainTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionTerrain>);

struct CCollisionBox
{
  BoundingBox boundingBox;

  static constexpr ComponentTypeId TypeId = CCollisionBoxTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionBox>);

struct CCollisionCapsule
{
  Capsule capsule;

  static constexpr ComponentTypeId TypeId = CCollisionCapsuleTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionCapsule>);

// TODO: Rename to ovoid?
struct CCollisionSphere
{
  Ovoid ovoid;

  static constexpr ComponentTypeId TypeId = CCollisionSphereTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionSphere>);

struct CCollisionCylinder
{
  Cylinder cylinder;

  static constexpr ComponentTypeId TypeId = CCollisionCylinderTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionCylinder>);

struct CCollisionRotational
{
  Mat3x3f inverseInertialTensor;
  std::array<Force, MAX_FORCES> torques;
  Vec3f angularAcceleration;
  Vec3f angularVelocity;
  Vec3f resolverDeltaAngularV;

  static constexpr ComponentTypeId TypeId = CCollisionRotationalTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionRotational>);

// TODO: Separate out rotation related fields
struct CCollisionDynamic
{
  float inverseMass = 1.f;
  std::array<Force, MAX_FORCES> linearForces;
  Vec3f linearAcceleration;
  Vec3f linearVelocity;
  Vec3f resolverDeltaLinearV;
  uint16_t resolverNumAdjustments = 0;
  uint16_t framesIdle = 0;
  // TODO: Use bitfield
  bool idle = false;

  static constexpr ComponentTypeId TypeId = CCollisionDynamicTypeId;
};

static_assert(std::is_trivially_copyable_v<CCollisionDynamic>);

enum class CollisionComponentType
{
  StaticBox,
  DynamicBox,
  TerrainChunk,
  Aggregate,
  Capsule,
  Polyhedron,
  Sphere,
  Cylinder
};

struct DDynamicBox
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionDynamic,
    CCollisionRotational, CCollisionBox
  >;

  float inverseMass = 1.f;
  float restitution = 0.2f;
  float friction = 0.4f;
  BoundingBox boundingBox;
};

struct DStaticBox
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CGlobalTransform, CCollision, CCollisionBox
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

// For now, spheres are static
// TODO: Rename to ovoid?
struct DSphere
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CGlobalTransform, CCollision, CCollisionSphere
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  Ovoid ovoid;
};

// For now, cylinders are static
struct DCylinder
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CGlobalTransform, CCollision, CCollisionCylinder
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  Cylinder cylinder;
};

// For now, polyhedra are static
struct DPolyhedron
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CGlobalTransform, CCollision
  >;

  float restitution = 0.2f;
  float friction = 0.4f;
  std::vector<Vec3f> vertices;
  std::vector<uint16_t> indices;
};

struct DAggregate
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CGlobalTransform
  >;

  std::vector<DStaticBox> boxes;
  std::vector<Mat4x4f> boxTransforms;
  std::vector<DPolyhedron> polyhedra;
  std::vector<Mat4x4f> polyhedraTransforms;
};

struct DCapsule
{
  using RequiredComponents = type_list<
    CSpatialFlags, CBoundingBox, CLocalTransform, CCollision, CCollisionDynamic, CCollisionCapsule
  >;

  float inverseMass = 1.f;
  float restitution = 0.2f;
  float friction = 0.4f;
  Capsule capsule;
};

struct Intersection
{
  EntityId entityId = NULL_ENTITY_ID;
  float distance = 0.f;
};

class SysCollision : public System
{
  public:
    using System::addEntity;  // Silence clang warning -Woverloaded-virtual

    virtual void setInverseMass(EntityId id, float inverseMass) = 0;
    // TODO: function to apply force at offset
    virtual void applyForce(EntityId id, const Vec3f& force, float seconds) = 0;
    virtual void applyTorque(EntityId id, const Vec3f& torque, float seconds) = 0;
    virtual void setStationary(EntityId id) = 0;

    virtual void addEntity(EntityId id, const DStaticBox& data) = 0;
    virtual void addEntity(EntityId id, const DDynamicBox& data) = 0;
    virtual void addEntity(EntityId id, const DTerrainChunk& data) = 0;
    virtual void addEntity(EntityId id, const DPolyhedron& data) = 0;
    virtual void addEntity(EntityId id, const DSphere& data) = 0;
    virtual void addEntity(EntityId id, const DCylinder& data) = 0;
    virtual void addEntity(EntityId id, const DCapsule& data) = 0;
    virtual void addEntity(EntityId id, const DAggregate& data) = 0;

    virtual const std::vector<EntityId>& getAggregateChildren(EntityId entityId) const = 0;
    virtual EntityId addPartToAggregate(EntityId entityId, CollisionComponentType type) = 0;

    virtual CollisionComponentType componentType(EntityId entityId) const = 0;

    virtual std::vector<Intersection> getIntersecting(const Vec3f& rayStart,
      const Vec3f& rayEnd) const = 0;

    static const SystemId id = Systems::Collision;
};

using SysCollisionPtr = std::unique_ptr<SysCollision>;

class EventSystem;

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

}
