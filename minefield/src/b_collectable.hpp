#pragma once

#include <fge/sys_behaviour.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

// Entity must have an animation component with a "collect" animation
fge::DBehaviourPtr createBCollectable(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  fge::EntityId entityId, fge::EntityId playerId, int value);
