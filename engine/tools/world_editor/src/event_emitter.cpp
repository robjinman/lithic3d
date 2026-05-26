#include "event_emitter.hpp"
#include <unordered_map>
#include <set>
#include <cassert>

namespace
{

class EventHandleImpl : public EventHandle_
{
  public:
    EventHandleImpl(EventEmitter::HandleId id, EventEmitter& emitter)
      : m_emitter(emitter)
      , m_id(id)
    {}

    ~EventHandleImpl() override;

  private:
    EventEmitter& m_emitter;
    EventEmitter::HandleId m_id;
};

struct Subscription
{
  EventId eventId;
  EventHandler handler;
};

class EventEmitterImpl : public EventEmitter
{
  public:
    EventHandle subscribe(EventId eventId, const EventHandler& handler) override;
    void unsubscribe(HandleId handleId) override;
    void raise(EventId eventId) override;

  private:
    std::unordered_map<HandleId, Subscription> m_subscriptions;
    std::unordered_map<EventId, std::set<HandleId>> m_eventHandlers;
};

EventHandleImpl::~EventHandleImpl()
{
  m_emitter.unsubscribe(m_id);
}

EventHandle EventEmitterImpl::subscribe(EventId eventId, const EventHandler& handler)
{
  static HandleId nextHandleId = 0;

  auto handleId = nextHandleId++;

  m_subscriptions.insert({ handleId, Subscription{
    .eventId = eventId,
    .handler = handler
  }});

  m_eventHandlers[eventId].insert(handleId);

  return std::make_unique<EventHandleImpl>(handleId, *this);
}

void EventEmitterImpl::unsubscribe(HandleId handleId)
{
  auto i = m_subscriptions.find(handleId);
  assert(i != m_subscriptions.end());

  auto& subscription = i->second;

  m_eventHandlers.at(subscription.eventId).erase(handleId);
  m_subscriptions.erase(i);
}

void EventEmitterImpl::raise(EventId eventId)
{
  auto& handlers = m_eventHandlers[eventId];
  for (auto handler : handlers) {
    m_subscriptions.at(handler).handler();
  }
}

} // namespace

EventHandle_::~EventHandle_() {}

EventEmitterPtr createEventEmitter()
{
  return std::make_unique<EventEmitterImpl>();
}
