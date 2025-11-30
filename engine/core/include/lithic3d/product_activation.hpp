#pragma once

#include "event_system.hpp"

namespace lithic3d
{

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
namespace render { struct BitmapFont; }
class Logger;
class Ecs;

ProductActivationPtr createProductActivation(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  const render::BitmapFont& font, Logger& logger);

} // namespace lithic3d
