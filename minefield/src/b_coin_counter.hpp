#pragma once

#include <fge/sys_behaviour.hpp>

namespace fge
{
class Ecs;
class EventSystem;
}

fge::BehaviourDataPtr createBCoinCounter(uint32_t coinsRequired, fge::Ecs& ecs,
  fge::EventSystem& eventSystem, fge::EntityId entityId);
