#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBNumericTile(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  const Vec2i& pos, int value);
