#pragma once

#include "input.hpp"
#include "sys_spatial.hpp"
#include "utils.hpp"
#include <functional>

namespace lithic3d
{

struct DUi;

class SysUi : public System
{
  public:
    using GroupId = uint32_t;

    virtual void addEntity(EntityId id, const DUi& data) = 0;
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

    static const SystemId id = UI_SYSTEM;
};

using SysUiPtr = std::unique_ptr<SysUi>;

struct CUi
{
  bool mouseOver = false;

  static constexpr ComponentType TypeId = CUiTypeId;
};

struct DUi
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform, CUi>;

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

} // namespace lithic3d
