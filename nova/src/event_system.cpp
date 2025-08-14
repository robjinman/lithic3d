#include "event_system.hpp"

namespace
{

class EventSystemImpl : public EventSystem
{
  public:
    Handle listen(HashedString category, EventHandlerFn handler) override;
    Handle listen(HashedString category, HashedString type, EventHandlerFn handler) override;
    void fireEvent(const Event& event) override;
    void forget(const Handle& handle) override;
};

EventSystem::Handle EventSystemImpl::listen(HashedString category, EventHandlerFn handler)
{
  // TODO
  return Handle{};
}

EventSystem::Handle EventSystemImpl::listen(HashedString category, HashedString type,
  EventHandlerFn handler)
{
  // TODO
  return Handle{};
}

void EventSystemImpl::fireEvent(const Event& event)
{

}

void EventSystemImpl::forget(const Handle& handle)
{

}

}

EventSystemPtr createEventSystem()
{
  return std::make_unique<EventSystemImpl>();
}
