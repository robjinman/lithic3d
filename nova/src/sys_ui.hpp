#pragma once

#include "ecs.hpp"
#include "event_system.hpp"
#include "input.hpp"
#include "utils.hpp"
#include "component_types.hpp"

const HashedString g_strUiItemActivate = hashString("ui_item_activate");
const HashedString g_strUiItemPrime = hashString("ui_item_prime");
const HashedString g_strUiItemGainFocus = hashString("ui_item_gain_focus");
const HashedString g_strUiItemLoseFocus = hashString("ui_item_lose_focus");
const HashedString g_strUiItemCancel = hashString("ui_item_cancel");

class EUiItemStateChange : public Event
{
  public:
    EUiItemStateChange(HashedString eventName, EntityId entityId)
      : Event(eventName)
      , entityId(entityId) {}

    EUiItemStateChange(HashedString eventName, EntityId entityId, const EntityIdSet& targets)
      : Event(eventName, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class UiData;

// Requires components:
//   CSpatialFlags
//   CGlobalTransform
//   CUi
class SysUi : public System
{
  public:
    using GroupId = uint32_t;

    virtual void addEntity(EntityId id, const UiData& data) = 0;
    virtual void setActiveGroup(GroupId id) = 0;

    virtual void sendInputEnd(EntityId id, UserInput input) = 0;
    virtual void sendInputCancel(EntityId id) = 0;
    virtual void sendInputBegin(EntityId id, UserInput input) = 0;
    virtual void sendUnfocus(EntityId id) = 0;
    virtual void sendFocus(EntityId id) = 0;

    virtual ~SysUi() = default;

    static GroupId nextGroupId()
    {
      static GroupId nextId = 1;
      return nextId++;
    }
};

using SysUiPtr = std::unique_ptr<SysUi>;

struct CUi
{
  bool mouseOver = false;

  static constexpr ComponentType TypeId = CUiTypeId;
};

struct UiData
{
  SysUi::GroupId group = 0;
  EntityId topSlot = NULL_ENTITY;
  EntityId rightSlot = NULL_ENTITY;
  EntityId bottomSlot = NULL_ENTITY;
  EntityId leftSlot = NULL_ENTITY;
  std::set<UserInput> inputFilter{};
  std::function<void()> onGainFocus = []() {};
  std::function<void()> onLoseFocus = []() {};
  std::function<void(const UserInput& input)> onInputBegin = [](const UserInput&) {};
  std::function<void(const UserInput& input)> onInputEnd = [](const UserInput&) {};
  std::function<void()> onInputCancel = []() {};
};

class Logger;

SysUiPtr createSysUi(Ecs& ecs, EventSystem& eventSystem, Logger& logger);
