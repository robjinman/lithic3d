#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

// Entity must have an animation component with a "collect" animation
BehaviourDataPtr createBCollectable(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, int value);
