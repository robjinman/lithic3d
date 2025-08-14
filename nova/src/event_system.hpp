#pragma once

#include "utils.hpp"
#include <functional>

struct Event
{
  public:
    Event(HashedString type)
      : m_type(type) {}

    virtual HashedString type() const;

  private:
    HashedString m_type;
};

using EventHandlerFn = std::function<void(const Event&)>;

class EventSystem
{
  public:
    class Handle
    {

    };

    virtual Handle listen(HashedString category, EventHandlerFn handler) = 0;
    virtual Handle listen(HashedString category, HashedString type, EventHandlerFn handler) = 0;
    virtual void fireEvent(const Event& event) = 0;
    virtual void forget(const Handle& handle) = 0;
};

using EventSystemPtr = std::unique_ptr<EventSystem>;

EventSystemPtr createEventSystem();
