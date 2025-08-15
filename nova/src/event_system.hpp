#pragma once

#include "utils.hpp"
#include <functional>
#include <memory>

class Event
{
  public:
    Event(HashedString name)
      : m_name(name) {}

    virtual HashedString name() const
    {
      return m_name;
    }

    virtual ~Event() {};

  private:
    HashedString m_name;
};

using EventHandlerFn = std::function<void(const Event&)>;

class EventSystem
{
  public:
    virtual void listen(HashedString name, EventHandlerFn handler) = 0;
    virtual void fireEvent(const Event& event) = 0;
};

using EventSystemPtr = std::unique_ptr<EventSystem>;

EventSystemPtr createEventSystem();
