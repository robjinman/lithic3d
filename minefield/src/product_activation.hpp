#pragma once

#include "event_system.hpp"

const HashedString g_strProductActivate = hashString("product_activate");

class EProductActivate : public Event
{
  public:
    EProductActivate()
      : Event(g_strProductActivate) {}
};

class ProductActivation
{
  public:
    virtual EntityId root() const = 0;

    virtual ~ProductActivation() = default;
};

using ProductActivationPtr = std::unique_ptr<ProductActivation>;

class Drm;
class Logger;

ProductActivationPtr createProductActivation(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  Logger& logger);
