#pragma once

#include "sys_behaviour.hpp"
#include "sys_animation.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, AnimationId throwAnimation);
