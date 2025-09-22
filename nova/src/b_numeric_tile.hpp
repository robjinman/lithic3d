#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;
class TimeService;

BehaviourDataPtr createBNumericTile(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, const Vec2i& pos, int value);
