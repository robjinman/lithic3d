#include "event_system.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <map>

namespace
{

class EventSystemImpl : public EventSystem
{
  public:
    EventSystemImpl(Logger& logger);

    void listen(HashedString name, EventHandlerFn handler) override;
    void fireEvent(const Event& event) override;

  private:
    Logger& m_logger;
    std::map<HashedString, std::vector<EventHandlerFn>> m_handlers;
};

EventSystemImpl::EventSystemImpl(Logger& logger)
  : m_logger(logger)
{
}

void EventSystemImpl::listen(HashedString name, EventHandlerFn handler)
{
  m_handlers[name].push_back(handler);
}

void EventSystemImpl::fireEvent(const Event& event)
{
  DBG_LOG(m_logger, STR("Event: " << event.toString()));

  auto i = m_handlers.find(event.name());
  if (i != m_handlers.end()) {
    for (auto& fn : i->second) {
      fn(event);
    }
  }
}

} // namespace

EventSystemPtr createEventSystem(Logger& logger)
{
  return std::make_unique<EventSystemImpl>(logger);
}
