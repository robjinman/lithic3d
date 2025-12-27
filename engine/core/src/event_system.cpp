#include "lithic3d/event_system.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/trace.hpp"
#include <map>
#include <list>

namespace lithic3d
{
namespace
{

class EventSystemImpl : public EventSystem
{
  public:
    explicit EventSystemImpl(Logger& logger);

    void listen(EventHandlerFn handler) override;
    void listen(HashedString name, EventHandlerFn handler) override;
    void raiseEvent(const Event& event) override;
    void scheduleEvent(Tick delay, EventPtr event) override;
    void processEvents() override;
    void dropEvents() override;

    ~EventSystemImpl() override;

  private:
    Logger& m_logger;
    std::map<HashedString, std::vector<EventHandlerFn>> m_handlers;
    std::vector<EventHandlerFn> m_globalHandlers;
    std::list<std::pair<EventPtr, Tick>> m_scheduledEvents;

    void processScheduledEvents();
};

EventSystemImpl::EventSystemImpl(Logger& logger)
  : m_logger(logger)
{
}

void EventSystemImpl::dropEvents()
{
  m_scheduledEvents.clear();
}

void EventSystemImpl::listen(EventHandlerFn handler)
{
  m_globalHandlers.push_back(handler);
}

void EventSystemImpl::listen(HashedString name, EventHandlerFn handler)
{
  m_handlers[name].push_back(handler);
}

void EventSystemImpl::raiseEvent(const Event& event)
{
  DBG_LOG(m_logger, STR("Event: " << event.toString()));

  std::vector<EventHandlerFn> handlers;

  for (auto& handler : m_globalHandlers) {
    handlers.push_back(handler);
  }

  auto i = m_handlers.find(event.name);
  if (i != m_handlers.end()) {
    for (auto& handler : i->second) {
      handlers.push_back(handler);
    }
  }

  for (auto handler : handlers) {
    handler(event);
  }
}

void EventSystemImpl::scheduleEvent(Tick delay, EventPtr event)
{
  m_scheduledEvents.push_back(std::make_pair(std::move(event), delay));
}

void EventSystemImpl::processEvents()
{
  processScheduledEvents();
}

void EventSystemImpl::processScheduledEvents()
{
  std::vector<EventPtr> toProcess;

  for (auto i = m_scheduledEvents.begin(); i != m_scheduledEvents.end();) {
    if (--i->second == 0) {
      toProcess.push_back(std::move(i->first));
      i = m_scheduledEvents.erase(i);
    }
    else {
      ++i;
    }
  }

  for (auto& event : toProcess) {
    raiseEvent(*event);
  }
}

EventSystemImpl::~EventSystemImpl()
{
  DBG_TRACE(m_logger);
}

} // namespace

EventSystemPtr createEventSystem(Logger& logger)
{
  return std::make_unique<EventSystemImpl>(logger);
}

} // namespace lithic3d
