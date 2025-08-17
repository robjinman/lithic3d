#pragma once

#include <memory>
#include <set>
#include "event_system.hpp"
#include "units.hpp"
#include "ecs.hpp"

class GameEvent : public Event
{
  public:
    GameEvent(HashedString name)
      : Event(hashString("game"))
      , name(name) {}

    GameEvent(HashedString name, const std::set<EntityId>& targets)
      : Event(hashString("game"))
      , name(name)
      , targets(targets) {}

    HashedString name;
    std::set<EntityId> targets;
};

class InputState;

// Most systems will store all of their component data in the World object, but don't have to.
class System
{
  public:
    virtual void removeEntity(EntityId entityId) = 0;
    virtual bool hasEntity(EntityId entityId) const = 0;
    virtual void update(Tick tick, const InputState& inputState) = 0;
    virtual void processEvent(const GameEvent& event) = 0;

    virtual ~System() {}
};

using SystemPtr = std::unique_ptr<System>;
