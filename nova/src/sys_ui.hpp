#pragma once

#include "ecs.hpp"
#include "event_system.hpp"
#include "math.hpp"
#include "utils.hpp"
#include "component_types.hpp"

const HashedString g_strUiItemActivate = hashString("ui_item_activate");
const HashedString g_strUiItemMouseEnter = hashString("ui_item_mouse_enter");
const HashedString g_strUiItemMouseExit = hashString("ui_item_mouse_exit");

class EUiItemActivate : public Event
{
  public:
    EUiItemActivate(EntityId entityId)
      : Event(g_strUiItemActivate)
      , entityId(entityId) {}

    EUiItemActivate(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strUiItemActivate, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class EUiItemMouseEnter : public Event
{
  public:
    EUiItemMouseEnter(EntityId entityId)
      : Event(g_strUiItemMouseEnter)
      , entityId(entityId) {}

    EUiItemMouseEnter(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strUiItemMouseEnter, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class EUiItemMouseExit : public Event
{
  public:
    EUiItemMouseExit(EntityId entityId)
      : Event(g_strUiItemMouseExit)
      , entityId(entityId) {}

    EUiItemMouseExit(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strUiItemMouseExit, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

struct UiData
{
};

struct CUi
{
  bool mouseOver = false;
  bool mouseButtonDown = false;

  static constexpr ComponentType TypeId = CUiTypeId;
};

class SysUi : public System
{
  public:
    virtual void addEntity(EntityId id, const UiData& data) = 0;

    virtual ~SysUi() = default;
};

using SysUiPtr = std::unique_ptr<SysUi>;

class Logger;

SysUiPtr createSysUi(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger);
