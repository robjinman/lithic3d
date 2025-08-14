#pragma once

#include "utils.hpp"
#include <memory>

using EntityId = size_t;

class Event
{
  public:
    Event(hashedString_t name);

    hashedString_t name() const;

    virtual ~Event() {}
};

class Component
{
  public:
    virtual void update() = 0;
    virtual void notify(const Event& event) = 0;

    virtual ~Component() {}
};

using ComponentPtr = std::unique_ptr<Component>;
