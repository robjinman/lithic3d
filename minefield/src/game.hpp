#pragma once

#include "input.hpp"
#include <memory>

const float GAME_AREA_ASPECT = 630.f / 480.f;

class Game
{
  public:
    virtual void onKeyDown(KeyboardKey key) = 0;
    virtual void onKeyUp(KeyboardKey key) = 0;
    virtual void onButtonDown(GamepadButton button) = 0;
    virtual void onButtonUp(GamepadButton button) = 0;
    virtual void onMouseButtonDown() = 0;
    virtual void onMouseButtonUp() = 0;
    virtual void onMouseMove(const Vec2f& pos, const Vec2f& delta) = 0;
    virtual void onLeftStickMove(const Vec2f& delta) = 0;
    virtual void onRightStickMove(const Vec2f& delta) = 0;
    virtual void onWindowResize(uint32_t w, uint32_t h) = 0;
    virtual void hideMobileControls() = 0;
    virtual void showMobileControls() = 0;
    virtual bool update() = 0;

    virtual ~Game() = default;
};

using GamePtr = std::unique_ptr<Game>;

namespace render { class Renderer; }
class AudioSystem;
class FileSystem;
class Logger;

GamePtr createGame(render::Renderer& renderer, AudioSystem& audioSystem, FileSystem& fileSystem,
  Logger& logger);
