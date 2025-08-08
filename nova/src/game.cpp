#include "game.hpp"
#include "player.hpp"
#include "render_system.hpp"
#include "time.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "units.hpp"
#include <set>
#undef max
#undef min

namespace
{

KeyboardKey buttonToKey(GamepadButton button)
{
  switch (button) {
    case GamepadButton::A: return KeyboardKey::E;
    case GamepadButton::Y: return KeyboardKey::F;
    case GamepadButton::X: return KeyboardKey::K;
    case GamepadButton::B: return KeyboardKey::R;
    // ...
    default: return KeyboardKey::Unknown;
  }
}

class GameImpl : public Game
{
  public:
    GameImpl(PlayerPtr player, RenderSystem& renderSystem, Logger& logger);

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onButtonDown(GamepadButton button) override;
    void onButtonUp(GamepadButton button) override;
    void onMouseMove(const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f& delta) override;
    void onRightStickMove(const Vec2f& delta) override;
    void update() override;

  private:
    Logger& m_logger;
    RenderSystem& m_renderSystem;
    PlayerPtr m_player;
    std::set<KeyboardKey> m_keysPressed;
    Vec2f m_mouseDelta;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    size_t m_frame = 0;
    double m_measuredFrameRate = 0;

    void measureFrameRate();
    void processKeyboardInput();
    void processMouseInput();
};

GameImpl::GameImpl(PlayerPtr player, RenderSystem& renderSystem, Logger& logger)
  : m_logger(logger)
  , m_renderSystem(renderSystem)
  , m_player(std::move(player))
{
}

void GameImpl::onKeyDown(KeyboardKey key)
{
  m_keysPressed.insert(key);

  switch (key) {
    case KeyboardKey::F:{
      m_logger.info(STR("Simulation frame rate: " << m_measuredFrameRate));
      break;
    }
    default: break;
  }
}

void GameImpl::onKeyUp(KeyboardKey key)
{
  m_keysPressed.erase(key);
}

void GameImpl::onButtonDown(GamepadButton button)
{
  onKeyDown(buttonToKey(button));
}

void GameImpl::onButtonUp(GamepadButton button)
{
  onKeyUp(buttonToKey(button));
}

void GameImpl::onMouseMove(const Vec2f& delta)
{
  m_mouseDelta = delta;
}

void GameImpl::onLeftStickMove(const Vec2f& delta)
{
  m_leftStickDelta = delta;
}

void GameImpl::onRightStickMove(const Vec2f& delta)
{

}

void GameImpl::processKeyboardInput()
{

}

void GameImpl::processMouseInput()
{

}

void GameImpl::measureFrameRate()
{
  ++m_frame;
  if (m_frame % TARGET_FRAME_RATE == 0) {
    m_measuredFrameRate = TARGET_FRAME_RATE / m_timer.elapsed();
    m_timer.reset();
  }
}

void GameImpl::update()
{
  measureFrameRate();
  processKeyboardInput();
  processMouseInput();
}

} // namespace

GamePtr createGame(PlayerPtr player, RenderSystem& renderSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(std::move(player), renderSystem, logger);
}
