#pragma once

#include "ecs.hpp"
#include "component_types.hpp"
#include "event_system.hpp"
#include "systems.hpp"
#include <unordered_set>

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

// Requires components:
//   CSpatialFlags
//   CLocalTransform
//   CGlobalTransform
struct DSpatial
{
  Mat4x4f transform = identityMatrix<float, 4>();
  EntityId parent = NULL_ENTITY;
  bool enabled = true;
};

struct CLocalTransform
{
  Mat4x4f transform = identityMatrix<float, 4>();

  static constexpr ComponentType TypeId = CLocalTransformTypeId;
};

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<float, 4>();

  static constexpr ComponentType TypeId = CGlobalTransformTypeId;
};

struct CSpatialFlags
{
  bool enabled = true; // TODO: Replace with bitset
  bool parentEnabled = true;

  static constexpr ComponentType TypeId = CSpatialFlagsTypeId;
};

class SysSpatial : public System
{
  public:
    virtual EntityId root() const = 0;
    virtual void addEntity(EntityId entityId, const DSpatial& data) = 0;
    virtual void setEnabled(EntityId entityId, bool enabled) = 0;

    // TODO: Replace with proper frustum culling
    virtual std::unordered_set<EntityId> getIntersecting(const std::vector<Vec2f>& poly) const = 0;

    virtual ~SysSpatial() = default;

    static const SystemId id = SPATIAL_SYSTEM;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(ComponentStore& componentStore, EventSystem& eventSystem);

} // namespace lithic3d
