#pragma once

#include <fge/sys_behaviour.hpp>
#include <fge/sys_animation.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

fge::BehaviourDataPtr createBStick(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  fge::EntityId entityId, fge::EntityId playerId, fge::AnimationId throwAnimation);
