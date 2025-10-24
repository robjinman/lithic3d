#pragma once

#include <fge/event_system.hpp>

const fge::HashedString g_strMenuItemActivate = fge::hashString("menu_item_activate");
const fge::HashedString g_strSubmenuExit = fge::hashString("submenu_exit");

class EMenuItemActivate : public fge::Event
{
  public:
    EMenuItemActivate(fge::EntityId entityId)
      : Event(g_strMenuItemActivate)
      , entityId(entityId) {}

    EMenuItemActivate(fge::EntityId entityId, const fge::EntityIdSet& targets)
      : Event(g_strMenuItemActivate, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " (" << "entityId = " << entityId << ")");
    }

    fge::EntityId entityId;
};

class ESubmenuExit : public fge::Event
{
  public:
    ESubmenuExit()
      : Event(g_strSubmenuExit) {}
};

struct InputState;

class MenuSystem
{
  public:
    virtual fge::EntityId root() const = 0;

    virtual void showMainMenu() = 0;
    virtual void showPauseMenu() = 0;

    virtual void update() = 0;

    virtual fge::EntityId startGameBtn() const = 0;
    virtual fge::EntityId resumeBtn() const = 0;
    virtual fge::EntityId quitToMainMenuBtn() const = 0;
    virtual fge::EntityId quitGameBtn() const = 0;

    virtual float sfxVolume() const = 0;
    virtual float musicVolume() const = 0;
    virtual uint32_t difficultyLevel() const = 0;

    virtual ~MenuSystem() = default;
};

using MenuSystemPtr = std::unique_ptr<MenuSystem>;

namespace fge
{
class Ecs;
class EventSystem;
class Logger;
}

class GameOptionsManager;

MenuSystemPtr createMenuSystem(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  const GameOptionsManager& optons, fge::Logger& logger, bool hasQuitButton);
