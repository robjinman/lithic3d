#pragma once

#include <memory>
#include <set>
#include <string>
#include "event_system.hpp"
#include "units.hpp"
#include "component_store.hpp"

using EntityIdSet = std::set<EntityId>;

class GameEvent : public Event
{
  public:
    explicit GameEvent(HashedString name)
      : Event(hashString("game"))
      , name(name) {}

    GameEvent(HashedString name, const EntityIdSet& targets)
      : Event(hashString("game"))
      , name(name)
      , targets(targets) {}

    std::string toString() const override
    {
      std::stringstream ss;
      ss << getHashedString(name) << ", targets = [ ";
      for (auto id : targets) {
        ss << id << " ";
      }
      ss << "]";
      return ss.str();
    }

    HashedString name;
    EntityIdSet targets;
};

// Some systems may choose to store their components in the component store for efficiency, but that
// is optional.

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
