#pragma once

#include <cstdint>
#include <functional>
#include <memory>

using EventId = uint32_t;

class EventHandle_
{
  public:
    virtual ~EventHandle_() = 0;
};

using EventHandle = std::unique_ptr<EventHandle_>;

using EventHandler = std::function<void()>;

class EventEmitter
{
  public:
    using HandleId = uint32_t;

    virtual EventHandle subscribe(EventId event, const EventHandler& handler) = 0;
    virtual void unsubscribe(HandleId handleId) = 0;
    virtual void raise(EventId eventId) = 0;

    virtual ~EventEmitter() = default;
};

using EventEmitterPtr = std::unique_ptr<EventEmitter>;

EventEmitterPtr createEventEmitter();
