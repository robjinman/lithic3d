#pragma once

#include "system.hpp"

class EventSystem;
class World;
class SysGrid;
class SysRender;
class SysBehaviour;

EntityId constructPlayer(EventSystem& eventSystem, World& world, SysGrid& sysGrid,
  SysRender& sysRender, SysBehaviour& sysBehaviour);

