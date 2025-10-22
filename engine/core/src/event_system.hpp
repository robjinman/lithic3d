#pragma once

#include "ecs.hpp"
#include <functional>
#include <memory>

class Event
{
  public:
    explicit Event(HashedString name)
      : name(name) {}

    Event(HashedString name, const EntityIdSet& targets)
      : name(name)
      , targets(targets) {}

    virtual std::string toString() const
    {
      std::stringstream ss;
      ss << getHashedString(name);
      if (targets.has_value()) {
        ss << ", targets = [ ";
        for (auto id : targets.value()) {
          ss << id << " ";
        }
        ss << "]";
      }
      return ss.str();
    }

    virtual ~Event() = default;

    HashedString name;
    std::optional<EntityIdSet> targets;
};

using EventPtr = std::unique_ptr<Event>;

#define SIMPLE_EVENT(typeName, name) \
  struct typeName : public Event {  typeName() : Event(name) {} };

using EventHandlerFn = std::function<void(const Event&)>;

class EventSystem
{
  public:
    virtual void listen(EventHandlerFn handler) = 0;
    virtual void listen(HashedString name, EventHandlerFn handler) = 0;
    virtual void raiseEvent(const Event& event) = 0;
    virtual void scheduleEvent(Tick delay, EventPtr event) = 0;
    virtual void processEvents() = 0;
    virtual void dropEvents() = 0;

    virtual ~EventSystem() = default;
};

using EventSystemPtr = std::unique_ptr<EventSystem>;

class Logger;

EventSystemPtr createEventSystem(Logger& logger);
