#pragma once

#include "sys_behaviour.hpp"
#include "math.hpp"

class SysGrid;
class SysRender;
class EventSystem;

CBehaviourPtr createCollectableBehaviour(const SysGrid& sysGrid, SysRender& sysRender,
  EventSystem& eventSystem, EntityId entityId, EntityId playerId, int value);
