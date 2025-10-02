#pragma once

#include "event_system.hpp"

const HashedString g_strMenuItemActivate = hashString("menu_item_activate");

class EMenuItemActivate : public Event
{
  public:
    EMenuItemActivate(EntityId entityId)
      : Event(g_strMenuItemActivate)
      , entityId(entityId) {}

    EMenuItemActivate(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strMenuItemActivate, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

class InputState;

class MenuSystem
{
  public:
    virtual EntityId root() const = 0;

    virtual void showMainMenu() = 0;
    virtual void showPauseMenu() = 0;

    virtual void update() = 0;

    virtual EntityId startGameBtn() const = 0;
    virtual EntityId resumeBtn() const = 0;
    virtual EntityId quitToMainMenuBtn() const = 0;
    virtual EntityId quitGameBtn() const = 0;

    virtual float sfxVolume() const = 0;
    virtual float musicVolume() const = 0;
    virtual uint32_t difficultyLevel() const = 0;

    virtual ~MenuSystem() = default;
};

using MenuSystemPtr = std::unique_ptr<MenuSystem>;

class Ecs;
class EventSystem;
class Logger;

MenuSystemPtr createMenuSystem(Ecs& ecs, EventSystem& eventSystem, Logger& logger);
