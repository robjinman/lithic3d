#pragma once

#include "entity.hpp"

class WorldGrid;
class CRender;

ComponentPtr createCollectableComponent(EntityId entityId, CRender& renderComp,
  EntityId playerId, const WorldGrid& worldGrid, EventSystem& eventSystem, int value);
