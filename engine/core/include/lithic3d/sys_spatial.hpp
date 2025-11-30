#pragma once

#include "ecs.hpp"
#include "component_types.hpp"
#include "event_system.hpp"
#include "systems.hpp"
#include "spatial_container.hpp"

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
};

struct CLocalTransform
{
  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentType TypeId = CLocalTransformTypeId;
};

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentType TypeId = CGlobalTransformTypeId;
};

struct CSpatialFlags
{
  bool enabled = true; // TODO: Replace with bitset
  bool parentEnabled = true;

  // TODO: Static

  static constexpr ComponentType TypeId = CSpatialFlagsTypeId;
};

struct CBoundingBox
{
  Aabb modelSpaceAabb;
  Aabb worldSpaceAabb;

  static constexpr ComponentType TypeId = CBoundingBoxTypeId;
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
    virtual void setEnabled(EntityId entityId, bool enabled) = 0;

    virtual void transformEntity(EntityId id, const Mat4x4f& m) = 0;
    virtual void setEntityTransform(EntityId id, const Mat4x4f& m) = 0;

    virtual EntityIdSet getIntersecting(const Frustum& frustum) const = 0;

    virtual ~SysSpatial() = default;

    static const SystemId id = SPATIAL_SYSTEM;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(Ecs& ecs, EventSystem& eventSystem);

} // namespace lithic3d
