#pragma once

#include "event_system.hpp"
#include "math.hpp"
#include "system.hpp"
#include "utils.hpp"

const HashedString g_strEntityStepOn = hashString("entity_step_on");
const HashedString g_strItemCollect = hashString("item_collect");
const HashedString g_strRequestDeletion = hashString("request_deletion");
const HashedString g_strAnimationFinished = hashString("animation_finished");

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

class EAnimationFinished : public GameEvent
{
  public:
    EAnimationFinished(EntityId entityId, HashedString name)
      : GameEvent(g_strAnimationFinished)
      , entityId(entityId)
      , animationName(name) {}

    EAnimationFinished(EntityId entityId, HashedString name, const EntityIdSet& targets)
      : GameEvent(g_strAnimationFinished, targets)
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
