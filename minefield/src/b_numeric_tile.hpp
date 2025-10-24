#pragma once

#include <fge/sys_behaviour.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

fge::BehaviourDataPtr createBNumericTile(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  fge::EntityId entityId, const fge::Vec2i& pos, int value);
