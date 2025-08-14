#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "system.hpp"
#include "utils.hpp"

class EPlayerMoved : public GameEvent
{
  public:
    EPlayerMoved()
      : GameEvent(hashString("player_moved"))
    {}

    EPlayerMoved(const std::set<EntityId>& targets)
      : GameEvent(hashString("player_moved"), targets)
    {}

    Vec2i fromPos;
    Vec2i toPos;
};

struct EItemCollected : public GameEvent
{
  EItemCollected(EntityId entityId, int value);
};

struct ERequestDeletion : public Event
{
    ERequestDeletion(EntityId entityId);
};
