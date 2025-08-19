#pragma once

#include "system.hpp"

class EventSystem;
class ComponentStore;
class SysGrid;
class SysRender;
class SysBehaviour;
class SysAnimation;

EntityId constructPlayer(EventSystem& eventSystem, ComponentStore& componentStore, SysGrid& sysGrid,
  SysRender& sysRender, SysBehaviour& sysBehaviour, SysAnimation& sysAnimation);

