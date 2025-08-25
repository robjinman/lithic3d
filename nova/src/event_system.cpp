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

    void listen(EventHandlerFn handler) override;
    void listen(HashedString name, EventHandlerFn handler) override;
    void queueEvent(EventPtr event) override;
    void processEvents() override;
    void dropEvents() override;

  private:
    Logger& m_logger;
    std::map<HashedString, std::vector<EventHandlerFn>> m_handlers;
    std::vector<EventHandlerFn> m_globalHandlers;
    std::vector<EventPtr> m_eventQueue;
};

EventSystemImpl::EventSystemImpl(Logger& logger)
  : m_logger(logger)
{
}

void EventSystemImpl::dropEvents()
{
  m_eventQueue.clear();
}

void EventSystemImpl::listen(EventHandlerFn handler)
{
  m_globalHandlers.push_back(handler);
}

void EventSystemImpl::listen(HashedString name, EventHandlerFn handler)
{
  m_handlers[name].push_back(handler);
}

void EventSystemImpl::queueEvent(EventPtr event)
{
  m_eventQueue.push_back(std::move(event));
}

void EventSystemImpl::processEvents()
{
  auto queue = std::move(m_eventQueue);
  m_eventQueue.clear();

  for (auto& eventPtr : queue) {
    auto& event = *eventPtr;
    DBG_LOG(m_logger, STR("Event: " << event.toString()));

    for (auto& fn : m_globalHandlers) {
      fn(event);
    }

    auto i = m_handlers.find(event.name);
    if (i != m_handlers.end()) {
      for (auto& fn : i->second) {
        fn(event);
      }
    }
  }
}

} // namespace

EventSystemPtr createEventSystem(Logger& logger)
{
  return std::make_unique<EventSystemImpl>(logger);
}
