#pragma once

#include "input.hpp"
#include "math.hpp"
#include <memory>
#include <filesystem>

namespace lithic3d
{

struct GameConfig
{
  std::string appDisplayName;
  std::string appShortName;
  std::string vendorShortName;
  uint32_t windowW = 0;
  uint32_t windowH = 0;
  uint32_t fullscreenResolutionW = 0;
  uint32_t fullscreenResolutionH = 0;
  bool captureMouse = false;
  std::filesystem::path shaderManifest;
  float drawDistance = 100.f; // In metres
};

class Game
{
  public:
    virtual float gameViewportAspectRatio() const = 0;
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

class Engine;

} // namespace lithic3d

// Game must define
lithic3d::GameConfig getGameConfig();
lithic3d::GamePtr createGame(lithic3d::Engine& engine);
