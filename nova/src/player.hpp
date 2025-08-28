#pragma once

#include "ecs.hpp"

class EventSystem;

EntityId constructPlayer(EventSystem& eventSystem, Ecs& ecs, EntityId worldRoot);
