#pragma once

#include "ecs.hpp"
#include "component_types.hpp"
#include "event_system.hpp"
#include "systems.hpp"
#include "loose_octree.hpp"
#include "utils.hpp"
#include <bitset>

namespace lithic3d
{

const HashedString g_strEntityEnable = hashString("entity_enable");

class EEntityEnable : public Event
{
  public:
    EEntityEnable(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strEntityEnable, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

struct Aabb
{
  Vec3f min;
  Vec3f max;

  inline void add(const Aabb& other)
  {
    min = {
      std::min(min[0], other.min[0]),
      std::min(min[1], other.min[1]),
      std::min(min[2], other.min[2])
    };
    max = {
      std::max(max[0], other.max[0]),
      std::max(max[1], other.max[1]),
      std::max(max[2], other.max[2])
    };
  }
};

Aabb transformAabb(const Aabb& aabb, const Mat4x4f& m);

struct CLocalTransform
{
  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentTypeId TypeId = CLocalTransformTypeId;
};

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentTypeId TypeId = CGlobalTransformTypeId;
};

namespace SpatialFlags
{
  enum : size_t
  {
    Enabled,
    ParentEnabled,
    Dirty
  };
}

struct CSpatialFlags
{
  std::bitset<32> flags{bitflag(SpatialFlags::Enabled) | bitflag(SpatialFlags::ParentEnabled)};

  static constexpr ComponentTypeId TypeId = CSpatialFlagsTypeId;
};

struct CBoundingBox
{
  Aabb modelSpaceAabb;
  Aabb worldSpaceAabb;

  static constexpr ComponentTypeId TypeId = CBoundingBoxTypeId;
};

struct DSpatial
{
  using RequiredComponents = type_list<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox
  >;

  Mat4x4f transform = identityMatrix<4>();
  EntityId parent = NULL_ENTITY_ID;
  bool enabled = true;
  Aabb aabb; // Model-space bounding box
};

class SysSpatial : public System
{
  public:  
    virtual EntityId root() const = 0;
    virtual void addEntity(EntityId id, const DSpatial& data) = 0;
    virtual void setEnabled(EntityId id, bool enabled) = 0;

    // TODO: Rethink terminology
    virtual const Mat4x4f& getLocalTransform(EntityId id) const = 0;
    virtual const Mat4x4f& getGlobalTransform(EntityId id) const = 0;
    virtual void transformEntitySelf(EntityId id, const Mat4x4f& m) = 0;
    virtual void transformEntityLocal(EntityId id, const Mat4x4f& m) = 0;
    virtual void transformEntityWorld(EntityId id, const Mat4x4f& m) = 0; // Slow
    virtual void setLocalTransform(EntityId id, const Mat4x4f& m) = 0;

    virtual EntityIdSet getIntersecting(const Frustum& frustum) const = 0;

    virtual ~SysSpatial() = default;

    virtual const LooseOctree& dbg_getOctree() const = 0;

    static const SystemId id = Systems::Spatial;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

} // namespace lithic3d
