#pragma once

#include <fge/sys_behaviour.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

fge::BehaviourDataPtr createBWanderer(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  fge::EntityId entityId, fge::EntityId playerId);
