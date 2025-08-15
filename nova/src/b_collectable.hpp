#pragma once

#include "sys_behaviour.hpp"
#include "math.hpp"

class SysBehaviour;
class EventSystem;

// Entity must have BAnimation behaviour with "collect" animation
CBehaviourPtr createBCollectable(SysBehaviour& SysBehaviour, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value);
