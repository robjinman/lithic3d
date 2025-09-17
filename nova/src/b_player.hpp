#pragma once

#include "sys_behaviour.hpp"

class Ecs;
class EventSystem;

BehaviourDataPtr createBPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId);
