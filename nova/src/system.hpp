#pragma once

#include <memory>
#include <set>
#include <string>
#include "event_system.hpp"
#include "units.hpp"
#include "ecs.hpp"

class GameEvent : public Event
{
  public:
    explicit GameEvent(HashedString name)
      : Event(hashString("game"))
      , name(name) {}

    GameEvent(HashedString name, const std::set<EntityId>& targets)
      : Event(hashString("game"))
      , name(name)
      , targets(targets) {}

    HashedString name;
    std::set<EntityId> targets;
};

class System
{
  public:
    virtual void removeEntity(EntityId entityId) = 0;
    virtual bool hasEntity(EntityId entityId) const = 0;
    virtual void update(Tick tick) = 0;
    virtual void processEvent(const GameEvent& event) = 0;

    virtual ~System() = default;
};

using SystemPtr = std::unique_ptr<System>;
