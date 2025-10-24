#pragma once

#include "ecs.hpp"
#include "input.hpp"
#include "utils.hpp"
#include "component_types.hpp"
#include <functional>

namespace fge
{

struct UiData;

// Requires components:
//   CSpatialFlags
//   CGlobalTransform
//   CUi
class SysUi : public System
{
  public:
    using GroupId = uint32_t;

    virtual void addEntity(EntityId id, const UiData& data) = 0;
    virtual void setActiveGroup(GroupId id, EntityId focusedItem = NULL_ENTITY) = 0;

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
  bool canReceiveFocus = true;
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

SysUiPtr createSysUi(Ecs& ecs, Logger& logger);

} // namespace fge
