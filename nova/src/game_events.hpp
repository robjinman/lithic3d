#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "system.hpp"
#include "utils.hpp"

const HashedString g_strPlayerMoved = hashString("player_moved");
const HashedString g_strItemCollected = hashString("item_collected");
const HashedString g_strRequestDeletion = hashString("request_deletion");
const HashedString g_strAnimationFinished = hashString("animation_finished");

class EPlayerMoved : public GameEvent
{
  public:
    EPlayerMoved(const Vec2i& fromPos, const Vec2i& toPos)
      : GameEvent(g_strPlayerMoved)
      , fromPos(fromPos)
      , toPos(toPos) {}

    EPlayerMoved(const Vec2i& fromPos, const Vec2i& toPos, const std::set<EntityId>& targets)
      : GameEvent(g_strPlayerMoved, targets)
      , fromPos(fromPos)
      , toPos(toPos) {}

    Vec2i fromPos;
    Vec2i toPos;
};

class EItemCollected : public GameEvent
{
  public:
    EItemCollected(EntityId entityId, uint32_t value)
      : GameEvent(g_strItemCollected)
      , entityId(entityId)
      , value(value) {}

    EItemCollected(EntityId entityId, uint32_t value, const std::set<EntityId>& targets)
      : GameEvent(g_strItemCollected, targets)
      , entityId(entityId)
      , value(value) {}

    EntityId entityId;
    uint32_t value;
};

class ERequestDeletion : public GameEvent
{
  public:
    ERequestDeletion(EntityId entityId)
      : GameEvent(g_strRequestDeletion)
      , entityId(entityId) {}

    ERequestDeletion(EntityId entityId, const std::set<EntityId>& targets)
      : GameEvent(g_strRequestDeletion, targets)
      , entityId(entityId) {}

    EntityId entityId;
};

class EAnimationFinished : public GameEvent
{
  public:
    EAnimationFinished(EntityId entityId, HashedString name)
      : GameEvent(g_strAnimationFinished)
      , entityId(entityId)
      , name(name) {}

    EAnimationFinished(EntityId entityId, HashedString name, const std::set<EntityId>& targets)
      : GameEvent(g_strAnimationFinished, targets)
      , entityId(entityId)
      , name(name) {}

    EntityId entityId;
    HashedString name;
};
