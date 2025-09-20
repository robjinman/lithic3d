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

using EventPtr = std::unique_ptr<Event>;

using EventHandlerFn = std::function<void(const Event&)>;

class EventSystem
{
  public:
    virtual void listen(EventHandlerFn handler) = 0;
    virtual void listen(HashedString name, EventHandlerFn handler) = 0;
    virtual void queueEvent(EventPtr event) = 0;
    virtual void scheduleEvent(Tick delay, EventPtr event) = 0;
    virtual void processEvents() = 0;
    virtual void dropEvents() = 0;
};

using EventSystemPtr = std::unique_ptr<EventSystem>;

class Logger;

EventSystemPtr createEventSystem(Logger& logger);
