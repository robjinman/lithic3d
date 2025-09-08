#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId);
