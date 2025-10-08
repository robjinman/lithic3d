#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "utils.hpp"

const HashedString g_strEntityEnter = hashString("entity_enter");
const HashedString g_strEntityLandOn = hashString("entity_land_on");
const HashedString g_strItemCollect = hashString("item_collect");
const HashedString g_strEntityExplode = hashString("entity_explode");
const HashedString g_strPlayerMove = hashString("player_move");
const HashedString g_strPlayerDeath = hashString("player_death");
const HashedString g_strPlayerVictorious = hashString("player_victorious");
const HashedString g_strEnterPortal = hashString("enter_portal");
const HashedString g_strTimerTick = hashString("timer_tick");
const HashedString g_strTimeout = hashString("timeout");
const HashedString g_strGoldTargetAttained = hashString("gold_target_attained");
const HashedString g_strAttack = hashString("attack");
const HashedString g_strToggleThrowingMode = hashString("toggle_throwing_mode");
const HashedString g_strThrow = hashString("throw");
const HashedString g_strTenSecondsRemaining = hashString("ten_seconds_remaining");
const HashedString g_strWandererAttack = hashString("wanderer_attack");

SIMPLE_EVENT(EPlayerDeath, g_strPlayerDeath)
SIMPLE_EVENT(EPlayerVictorious, g_strPlayerVictorious)
SIMPLE_EVENT(ETimeout, g_strTimeout)
SIMPLE_EVENT(EGoldTargetAttained, g_strGoldTargetAttained)
SIMPLE_EVENT(EEnterPortal, g_strEnterPortal)
SIMPLE_EVENT(ETenSecondsRemaining, g_strTenSecondsRemaining)
SIMPLE_EVENT(EWandererAttack, g_strWandererAttack)

class EEntityEnter : public Event
{
  public:
    EEntityEnter(EntityId entityId, const Vec2i& pos, const EntityIdSet& targets)
      : Event(g_strEntityEnter, targets)
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

class EEntityLandOn : public Event
{
  public:
    EEntityLandOn(EntityId entityId, const Vec2i& pos, const EntityIdSet& targets)
      : Event(g_strEntityLandOn, targets)
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

class EAttack : public Event
{
  public:
    EAttack(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strAttack, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class EToggleThrowingMode : public Event
{
  public:
    EToggleThrowingMode(EntityId stickId)
      : Event(g_strToggleThrowingMode)
      , stickId(stickId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "stickId = " << stickId << ")");
    }

    EntityId stickId;
};

class EThrow : public Event
{
  public:
    EThrow(EntityId stickId, int x, int y)
      : Event(g_strThrow)
      , stickId(stickId)
      , x(x)
      , y(y) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "stickId = " << stickId << ", "
        << "x = " << x << ", "
        << "y = " << y << ")");
    }

    EntityId stickId;
    int x;
    int y;
};
