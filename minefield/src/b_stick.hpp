#pragma once

#include <fge/sys_behaviour.hpp>
#include <fge/sys_animation_2d.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

fge::DBehaviourPtr createBStick(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  fge::EntityId entityId, fge::EntityId playerId, fge::Animation2dId throwAnimation);
