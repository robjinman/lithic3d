#pragma once

#include "ecs.hpp"
#include "component_types.hpp"
#include "event_system.hpp"

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

struct SpatialData
{
  Mat4x4f transform = identityMatrix<float_t, 4>();
  EntityId parent = NULL_ENTITY;
  bool enabled = true;
};

struct CLocalTransform
{
  Mat4x4f transform = identityMatrix<float_t, 4>();

  static constexpr ComponentType TypeId = CLocalTransformTypeId;
};

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<float_t, 4>();

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
    virtual void addEntity(EntityId entityId, const SpatialData& data) = 0;
    virtual void setEnabled(EntityId entityId, bool enabled) = 0;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(ComponentStore& componentStore, EventSystem& eventSystem);
