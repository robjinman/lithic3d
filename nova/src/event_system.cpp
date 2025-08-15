#include "event_system.hpp"
#include <map>

namespace
{

class EventSystemImpl : public EventSystem
{
  public:
    void listen(HashedString name, EventHandlerFn handler) override;
    void fireEvent(const Event& event) override;

  private:
    std::map<HashedString, std::vector<EventHandlerFn>> m_handlers;
};

void EventSystemImpl::listen(HashedString name, EventHandlerFn handler)
{
  m_handlers[name].push_back(handler);
}

void EventSystemImpl::fireEvent(const Event& event)
{
  auto i = m_handlers.find(event.name());
  if (i != m_handlers.end()) {
    for (auto& fn : i->second) {
      fn(event);
    }
  }
}

}

EventSystemPtr createEventSystem()
{
  return std::make_unique<EventSystemImpl>();
}
