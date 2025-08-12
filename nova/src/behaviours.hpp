#pragma once

#include "c_behaviour.hpp"
#include "math.hpp"

class WorldGrid;
class EntityManager;

struct EPlayerMoved : public Event
{
  Vec2i fromPos;
  Vec2i toPos;
};

struct EItemCollected : public Event
{
  EItemCollected(EntityId entityId, int value);
};

BehaviourPtr createCollectableBehaviour(EntityId entityId, EntityId playerId,
  const WorldGrid& worldGrid, EntityManager& entityManager, int value);
