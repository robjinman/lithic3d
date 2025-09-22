#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;
class TimeService;

BehaviourDataPtr createBWanderer(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, EntityId playerId);
