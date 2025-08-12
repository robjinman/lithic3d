#pragma once

#include <memory>
#include "component.hpp"

class Entity
{
  public:
    virtual bool hasComponent(hashedString_t name) const = 0;
    virtual Component& getComponent(hashedString_t name) = 0;
    virtual const Component& getComponent(hashedString_t name) const = 0;
    virtual void update() = 0;
    virtual void notify(const Event& event) = 0;
};

using EntityPtr = std::unique_ptr<Entity>;
