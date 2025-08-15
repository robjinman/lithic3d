#pragma once

#include "sys_behaviour.hpp"
#include "math.hpp"

class SysRender;
class EventSystem;

CBehaviourPtr createCollectableBehaviour(SysRender& sysRender, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, uint32_t value);
