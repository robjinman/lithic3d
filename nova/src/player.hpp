#pragma once

#include "system.hpp"

class EventSystem;
class SysGrid;
class SysRender;
class SysBehaviour;

EntityId constructPlayer(EventSystem& eventSystem, SysGrid& sysGrid, SysRender& sysRender,
  SysBehaviour& sysBehaviour);

