#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "utils.hpp"

const HashedString g_strEntityStepOn = hashString("entity_step_on");
const HashedString g_strItemCollect = hashString("item_collect");
const HashedString g_strAnimationFinish = hashString("animation_finish");
const HashedString g_strEntityExplode = hashString("entity_explode");
const HashedString g_strPlayerMove = hashString("player_move");
const HashedString g_strPlayerDeath = hashString("player_death");
const HashedString g_strTimerTick = hashString("timer_tick");

class EEntityStepOn : public Event
{
  public:
    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos)
      : Event(g_strEntityStepOn)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    EEntityStepOn(EntityId entityId, const Vec2i& fromPos, const Vec2i& toPos,
      const EntityIdSet& targets)
      : Event(g_strEntityStepOn, targets)
      , entityId(entityId)
      , fromPos(fromPos)
      , toPos(toPos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "fromPos = " << fromPos << ", "
        << "toPos = " << toPos << ")");
    }

    EntityId entityId;
    Vec2i fromPos;
    Vec2i toPos;
};

class EItemCollect : public Event
{
  public:
    EItemCollect(EntityId entityId, uint32_t value)
      : Event(g_strItemCollect)
      , entityId(entityId)
      , value(value) {}

    EItemCollect(EntityId entityId, uint32_t value, const EntityIdSet& targets)
      : Event(g_strItemCollect, targets)
      , entityId(entityId)
      , value(value) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "value = " << value << ")");
    }

    EntityId entityId;
    uint32_t value;
};

class EAnimationFinish : public Event
{
  public:
    EAnimationFinish(EntityId entityId, HashedString name)
      : Event(g_strAnimationFinish)
      , entityId(entityId)
      , animationName(name) {}

    EAnimationFinish(EntityId entityId, HashedString name, const EntityIdSet& targets)
      : Event(g_strAnimationFinish, targets)
      , entityId(entityId)
      , animationName(name) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "animationName = " << getHashedString(animationName) << ")");
    }

    EntityId entityId;
    HashedString animationName;
};

class EEntityExplode : public Event
{
  public:
    EEntityExplode(EntityId entityId, const Vec2i& pos)
      : Event(g_strEntityExplode)
      , entityId(entityId)
      , pos(pos) {}

    EEntityExplode(EntityId entityId, const Vec2i& pos, const EntityIdSet& targets)
      : Event(g_strEntityExplode, targets)
      , entityId(entityId)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "pos = " << pos << ")");
    }

    EntityId entityId;
    Vec2i pos;
};

class EPlayerDeath : public Event
{
  public:
    EPlayerDeath()
      : Event(g_strPlayerDeath) {}

    std::string toString() const override
    {
      return STR(Event::toString());
    }
};

class EPlayerMove : public Event
{
  public:
    EPlayerMove(const Vec2i& pos)
      : Event(g_strPlayerMove)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "pos = " << pos << ")");
    }

    Vec2i pos;
};

class ETimerTick : public Event
{
  public:
    ETimerTick(uint32_t timeRemaining)
      : Event(g_strTimerTick)
      , timeRemaining(timeRemaining) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "timeRemaining = " << timeRemaining << ")");
    }

    uint32_t timeRemaining;
};
