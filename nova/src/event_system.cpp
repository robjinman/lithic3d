#include "event_system.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <map>
#include <list>

namespace
{

class EventSystemImpl : public EventSystem
{
  public:
    EventSystemImpl(Logger& logger);

    void listen(EventHandlerFn handler) override;
    void listen(HashedString name, EventHandlerFn handler) override;
    void queueEvent(EventPtr event) override;
    void scheduleEvent(Tick delay, EventPtr event) override;
    void processEvents() override;
    void dropEvents() override;

  private:
    Logger& m_logger;
    std::map<HashedString, std::vector<EventHandlerFn>> m_handlers;
    std::vector<EventHandlerFn> m_globalHandlers;
    std::vector<EventPtr> m_eventQueue;
    std::list<std::pair<EventPtr, Tick>> m_scheduledEvents;

    void processScheduledEvents();
    void processEvent(const Event& event);
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

void EventSystemImpl::scheduleEvent(Tick delay, EventPtr event)
{
  m_scheduledEvents.push_back(std::make_pair(std::move(event), delay));
}

void EventSystemImpl::processEvents()
{
  auto queue = std::move(m_eventQueue);
  m_eventQueue.clear();

  for (auto& eventPtr : queue) {
    processEvent(*eventPtr);
  }

  processScheduledEvents();
}

void EventSystemImpl::processEvent(const Event& event)
{
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

void EventSystemImpl::processScheduledEvents()
{
  for (auto i = m_scheduledEvents.begin(); i != m_scheduledEvents.end();) {
    if (--i->second == 0) {
      processEvent(*i->first);
      i = m_scheduledEvents.erase(i);
    }
    else {
      ++i;
    }
  }
}

} // namespace

EventSystemPtr createEventSystem(Logger& logger)
{
  return std::make_unique<EventSystemImpl>(logger);
}
