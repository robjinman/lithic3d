#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "system.hpp"
#include "utils.hpp"

const HashedString g_strEntityStepOn = hashString("entity_step_on");
const HashedString g_strItemCollect = hashString("item_collect");
const HashedString g_strRequestDeletion = hashString("request_deletion");
const HashedString g_strAnimationFinish = hashString("animation_finish");
const HashedString g_strEntityExplode = hashString("entity_explode");
const HashedString g_strPlayerDeath = hashString("player_death");

class EEntityStepOn : public GameEvent
{
  public:
    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos)
      : GameEvent(g_strEntityStepOn)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos,
      const EntityIdSet& targets)
      : GameEvent(g_strEntityStepOn, targets)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString() << " ("
        << "entityId = " << entityId << ", "
        << "fromPos = " << fromPos << ", "
        << "toPos = " << toPos << ")");
    }

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

    EItemCollect(EntityId entityId, uint32_t value, const EntityIdSet& targets)
      : GameEvent(g_strItemCollect, targets)
      , entityId(entityId)
      , value(value) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString() << " ("
        << "entityId = " << entityId << ", "
        << "value = " << value << ")");
    }

    EntityId entityId;
    uint32_t value;
};

class ERequestDeletion : public GameEvent
{
  public:
    ERequestDeletion(EntityId entityId)
      : GameEvent(g_strRequestDeletion)
      , entityId(entityId) {}

    ERequestDeletion(EntityId entityId, const EntityIdSet& targets)
      : GameEvent(g_strRequestDeletion, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class EAnimationFinish : public GameEvent
{
  public:
    EAnimationFinish(EntityId entityId, HashedString name)
      : GameEvent(g_strAnimationFinish)
      , entityId(entityId)
      , animationName(name) {}

    EAnimationFinish(EntityId entityId, HashedString name, const EntityIdSet& targets)
      : GameEvent(g_strAnimationFinish, targets)
      , entityId(entityId)
      , animationName(name) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString() << " ("
        << "entityId = " << entityId << ", "
        << "animationName = " << getHashedString(animationName) << ")");
    }

    EntityId entityId;
    HashedString animationName;
};

class EEntityExplode : public GameEvent
{
  public:
    EEntityExplode(EntityId entityId, const Vec2i& pos)
      : GameEvent(g_strEntityExplode)
      , entityId(entityId)
      , pos(pos) {}

    EEntityExplode(EntityId entityId, const Vec2i& pos, const EntityIdSet& targets)
      : GameEvent(g_strEntityExplode, targets)
      , entityId(entityId)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString() << " ("
        << "entityId = " << entityId << ", "
        << "pos = " << pos << ")");
    }

    EntityId entityId;
    Vec2i pos;
};

class EPlayerDeath : public GameEvent
{
  public:
    EPlayerDeath()
      : GameEvent(g_strPlayerDeath) {}

    std::string toString() const override
    {
      return STR(GameEvent::toString());
    }
};
