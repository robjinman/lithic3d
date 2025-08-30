#pragma once

#include "component_store.hpp"

class InputState;

class MenuSystem
{
  public:
    virtual EntityId root() const = 0;

    virtual void showMainMenu() = 0;
    virtual void showPauseMenu() = 0;

    virtual EntityId startGameBtn() const = 0;
    virtual EntityId resumeBtn() const = 0;
    virtual EntityId quitToMainMenuBtn() const = 0;
    virtual EntityId quitGameBtn() const = 0;

    virtual float sfxVolume() const = 0;
    virtual float musicVolume() const = 0;
    virtual int difficultyLevel() const = 0;

    virtual ~MenuSystem() = default;
};

using MenuSystemPtr = std::unique_ptr<MenuSystem>;

class Ecs;
class EventSystem;
class Logger;

MenuSystemPtr createMenuSystem(Ecs& ecs, EventSystem& eventSystem, Logger& logger);
