#pragma once

#include <fge/event_system.hpp>
#include <fge/math.hpp>
#include <fge/utils.hpp>

const fge::HashedString g_strEntityEnter = fge::hashString("entity_enter");
const fge::HashedString g_strEntityLandOn = fge::hashString("entity_land_on");
const fge::HashedString g_strItemCollect = fge::hashString("item_collect");
const fge::HashedString g_strEntityExplode = fge::hashString("entity_explode");
const fge::HashedString g_strPlayerMove = fge::hashString("player_move");
const fge::HashedString g_strPlayerDeath = fge::hashString("player_death");
const fge::HashedString g_strPlayerVictorious = fge::hashString("player_victorious");
const fge::HashedString g_strEnterPortal = fge::hashString("enter_portal");
const fge::HashedString g_strTimerTick = fge::hashString("timer_tick");
const fge::HashedString g_strTimeout = fge::hashString("timeout");
const fge::HashedString g_strGoldTargetAttained = fge::hashString("gold_target_attained");
const fge::HashedString g_strAttack = fge::hashString("attack");
const fge::HashedString g_strToggleThrowingMode = fge::hashString("toggle_throwing_mode");
const fge::HashedString g_strThrow = fge::hashString("throw");
const fge::HashedString g_strTenSecondsRemaining = fge::hashString("ten_seconds_remaining");
const fge::HashedString g_strWandererAttack = fge::hashString("wanderer_attack");

SIMPLE_EVENT(EPlayerDeath, g_strPlayerDeath)
SIMPLE_EVENT(EPlayerVictorious, g_strPlayerVictorious)
SIMPLE_EVENT(ETimeout, g_strTimeout)
SIMPLE_EVENT(EGoldTargetAttained, g_strGoldTargetAttained)
SIMPLE_EVENT(EEnterPortal, g_strEnterPortal)
SIMPLE_EVENT(ETenSecondsRemaining, g_strTenSecondsRemaining)
SIMPLE_EVENT(EWandererAttack, g_strWandererAttack)

class EEntityEnter : public fge::Event
{
  public:
    EEntityEnter(fge::EntityId entityId, const fge::Vec2i& pos, const fge::EntityIdSet& targets)
      : Event(g_strEntityEnter, targets)
      , entityId(entityId)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "pos = " << pos << ")");
    }

    fge::EntityId entityId;
    fge::Vec2i pos;
};

class EEntityLandOn : public fge::Event
{
  public:
    EEntityLandOn(fge::EntityId entityId, const fge::Vec2i& pos, const fge::EntityIdSet& targets)
      : Event(g_strEntityLandOn, targets)
      , entityId(entityId)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "pos = " << pos << ")");
    }

    fge::EntityId entityId;
    fge::Vec2i pos;
};

class EItemCollect : public fge::Event
{
  public:
    EItemCollect(fge::EntityId entityId, uint32_t value)
      : Event(g_strItemCollect)
      , entityId(entityId)
      , value(value) {}

    EItemCollect(fge::EntityId entityId, uint32_t value, const fge::EntityIdSet& targets)
      : Event(g_strItemCollect, targets)
      , entityId(entityId)
      , value(value) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "value = " << value << ")");
    }

    fge::EntityId entityId;
    uint32_t value;
};

class EEntityExplode : public fge::Event
{
  public:
    EEntityExplode(fge::EntityId entityId, const fge::Vec2i& pos)
      : Event(g_strEntityExplode)
      , entityId(entityId)
      , pos(pos) {}

    EEntityExplode(fge::EntityId entityId, const fge::Vec2i& pos, const fge::EntityIdSet& targets)
      : Event(g_strEntityExplode, targets)
      , entityId(entityId)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ", "
        << "pos = " << pos << ")");
    }

    fge::EntityId entityId;
    fge::Vec2i pos;
};

class EPlayerMove : public fge::Event
{
  public:
    EPlayerMove(const fge::Vec2i& pos)
      : Event(g_strPlayerMove)
      , pos(pos) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "pos = " << pos << ")");
    }

    fge::Vec2i pos;
};

class ETimerTick : public fge::Event
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

class EAttack : public fge::Event
{
  public:
    EAttack(fge::EntityId entityId, const fge::EntityIdSet& targets)
      : Event(g_strAttack, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    fge::EntityId entityId;
};

class EToggleThrowingMode : public fge::Event
{
  public:
    EToggleThrowingMode(fge::EntityId stickId)
      : Event(g_strToggleThrowingMode)
      , stickId(stickId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "stickId = " << stickId << ")");
    }

    fge::EntityId stickId;
};

class EThrow : public fge::Event
{
  public:
    EThrow(fge::EntityId stickId, int x, int y)
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

    fge::EntityId stickId;
    int x;
    int y;
};
