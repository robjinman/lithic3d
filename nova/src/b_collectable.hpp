#pragma once

#include "sys_behaviour.hpp"
#include "math.hpp"

class SysAnimation;
class EventSystem;

// Entity must have an animation component with a "collect" animation
CBehaviourPtr createBCollectable(SysAnimation& sysAnimation, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value);
