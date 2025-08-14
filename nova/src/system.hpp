#pragma once

#include <memory>
#include <set>
#include <string>
#include "event_system.hpp"

using EntityId = size_t;

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

class System
{
  public:
    virtual void removeEntity(EntityId entityId) = 0;
    virtual bool hasEntity(EntityId entityId) const = 0;
    virtual void update() = 0;
    virtual void processEvent(const GameEvent& event) = 0;

    virtual ~System() {}

    static EntityId idFromString(const std::string& name);
    static EntityId nextId();

  private:
    static EntityId m_nextId;
    static std::set<EntityId> m_reservedIds;
};

using SystemPtr = std::unique_ptr<System>;
