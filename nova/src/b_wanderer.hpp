#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId);
