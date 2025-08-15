#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "system.hpp"
#include "utils.hpp"

const HashedString g_strPlayerStepOn = hashString("player_step_on");
const HashedString g_strItemCollect = hashString("item_collect");
const HashedString g_strRequestDeletion = hashString("request_deletion");
const HashedString g_strAnimationFinished = hashString("animation_finished");

class EEntityStepOn : public GameEvent
{
  public:
    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos)
      : GameEvent(g_strPlayerStepOn)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos,
      const std::set<EntityId>& targets)
      : GameEvent(g_strPlayerStepOn, targets)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    EntityId entityId;
    Vec2i fromPos;
    Vec2i toPos;
};

class EItemCollect : public GameEvent
{
  public:
    EItemCollect(EntityId entityId, uint32_t value)
      : GameEvent(g_strItemCollect)
      , entityId(entityId)
      , value(value) {}

    EItemCollect(EntityId entityId, uint32_t value, const std::set<EntityId>& targets)
      : GameEvent(g_strItemCollect, targets)
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
