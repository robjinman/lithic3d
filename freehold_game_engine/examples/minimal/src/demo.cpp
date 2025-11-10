#include <fge/engine.hpp>
#include <fge/game.hpp>
#include <fge/renderer.hpp>
#include <fge/ecs.hpp>
#include <fge/sys_render_2d.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/logger.hpp>

using fge::KeyboardKey;
using fge::GamepadButton;
using fge::Vec2f;
using fge::Tick;

namespace
{

class Demo : public fge::Game
{
  public:
    Demo(fge::Engine& engine);

    float gameViewportAspectRatio() const override;
    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onButtonDown(GamepadButton button) override;
    void onButtonUp(GamepadButton button) override;
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f& pos, const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f& delta) override;
    void onRightStickMove(const Vec2f& delta) override;
    void onWindowResize(uint32_t w, uint32_t h) override;
    void hideMobileControls() override;
    void showMobileControls() override;
    bool update() override;

  private:
    fge::Engine& m_engine;
    fge::InputState m_inputState;

    Tick m_currentTick = 0; // TODO: Remove
};

Demo::Demo(fge::Engine& engine)
  : m_engine(engine)
{
  m_engine.renderer().start(); // TODO: Move to engine?


}

float Demo::gameViewportAspectRatio() const
{
  return 1.4;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_engine.logger().info("Key down");
  m_inputState.keysPressed.insert(key);
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_engine.logger().info("Key up");
  m_inputState.keysPressed.erase(key);
}

void Demo::onButtonDown(GamepadButton button)
{
  m_engine.logger().info("Button down");
  // Map to KeyboardKey
}

void Demo::onButtonUp(GamepadButton button)
{
  m_engine.logger().info("Button up");
  // Map to KeyboardKey
}

void Demo::onMouseButtonDown()
{
  m_engine.logger().info("Mouse button down");
  m_inputState.mouseButtonsPressed.insert(fge::MouseButton::Left);
}

void Demo::onMouseButtonUp()
{
  m_engine.logger().info("Mouse button up");
  m_inputState.mouseButtonsPressed.erase(fge::MouseButton::Left);
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_engine.logger().info("Mouse move");
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
}

void Demo::onLeftStickMove(const Vec2f& delta)
{
  m_engine.logger().info("Left stick move");
}

void Demo::onRightStickMove(const Vec2f& delta)
{
  m_engine.logger().info("Right stick move");
}

void Demo::onWindowResize(uint32_t w, uint32_t h)
{
  m_engine.logger().info("Window resize");
}

void Demo::hideMobileControls()
{
  m_engine.logger().info("Hide mobile controls");
}

void Demo::showMobileControls()
{
  m_engine.logger().info("Show mobile controls");
}

bool Demo::update()
{
  // TODO: Replace with m_engine.update()
  m_engine.ecs().update(m_currentTick++, m_inputState);
  m_engine.eventSystem().processEvents();

  return true;
}

} // namespace

fge::GameConfig getGameConfig()
{
  return {
    .appDisplayName = "FGE Minimal Example",
    .appShortName = "minimal",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080
  };
}

fge::GamePtr createGame(fge::Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
