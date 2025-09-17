#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBCoinCounter(uint32_t coinsRequired, Ecs& ecs, EventSystem& eventSystem,
  EntityId entityId);
