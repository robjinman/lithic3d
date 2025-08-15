#include "game.hpp"
#include "sys_render.hpp"
#include "sys_grid.hpp"
#include "sys_behaviour.hpp"
#include "sys_ui.hpp"
#include "game_events.hpp"
#include "file_system.hpp"
#include "time.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "units.hpp"
#include "constants.hpp"
#include "player.hpp"
#include "input_state.hpp"
#include <set>
#undef max
#undef min

namespace
{

class GameImpl : public Game
{
  public:
    GameImpl(SysBehaviour& sysBehaviour, SysGrid& sysGrid, SysRender& sysRender,
      EventSystem& eventSystem, const FileSystem& fileSystem, Logger& logger);

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
    const FileSystem& m_fileSystem;
    EventSystem& m_eventSystem;
    SysRender& m_sysRender;
    SysGrid& m_sysGrid;
    SysBehaviour& m_sysBehaviour;
    InputState m_inputState;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    size_t m_frame = 0;
    double m_measuredFrameRate = 0;

    void measureFrameRate();
    void processKeyboardInput();
    void processMouseInput();

    void constructSky();
    void constructTrees();
    void constructFakeSoil();
    void constructSoil();
};

GameImpl::GameImpl(SysBehaviour& sysBehaviour, SysGrid& sysGrid, SysRender& sysRender,
  EventSystem& eventSystem, const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_eventSystem(eventSystem)
  , m_sysRender(sysRender)
  , m_sysGrid(sysGrid)
  , m_sysBehaviour(sysBehaviour)
{
  static auto strGame = hashString("game");
  static auto strUi = hashString("ui");

  m_eventSystem.listen(strGame, [&](const Event& e) {
    auto& gameEvent = dynamic_cast<const GameEvent&>(e);

    m_sysRender.processEvent(gameEvent);
    m_sysGrid.processEvent(gameEvent);
    m_sysBehaviour.processEvent(gameEvent);
  });

  m_eventSystem.listen(strUi, [&](const Event& e) {
    auto& uiEvent = dynamic_cast<const UiEvent&>(e);

    // TODO
  });

  constructPlayer(m_eventSystem, m_sysGrid, m_sysRender, m_sysBehaviour);
  constructSky();
  constructTrees();
  constructFakeSoil();
  constructSoil();
}

void GameImpl::onKeyDown(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::F: {
      m_logger.info(STR("Simulation frame rate: " << m_measuredFrameRate));
      break;
    }
    case KeyboardKey::Left: m_inputState.left = true; break;
    case KeyboardKey::Right: m_inputState.right = true; break;
    case KeyboardKey::Up: m_inputState.up = true; break;
    case KeyboardKey::Down: m_inputState.down = true; break;
    default: break;
  }
}

void GameImpl::onKeyUp(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::Left: m_inputState.left = false; break;
    case KeyboardKey::Right: m_inputState.right = false; break;
    case KeyboardKey::Up: m_inputState.up = false; break;
    case KeyboardKey::Down: m_inputState.down = false; break;
    default: break;
  }
}

void GameImpl::onButtonDown(GamepadButton button)
{
}

void GameImpl::onButtonUp(GamepadButton button)
{
}

void GameImpl::onMouseMove(const Vec2f& delta)
{
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

  m_sysBehaviour.update(m_inputState);
  m_sysGrid.update(m_inputState);
  m_sysRender.update(m_inputState);
}

void GameImpl::constructSky()
{
  auto id = System::nextId();

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 416.f;
  float_t wPx = 128.f;
  float_t hPx = 32.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  CRender render{
    .textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    },
    .size = Vec2f{ 1.3125f, 0.25f },
    .pos = Vec2f{ 0.f, 0.75f },
    .zIndex = 0
  };

  m_sysRender.addEntity(id, render);
}

void GameImpl::constructTrees()
{
  auto id = System::nextId();

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 352.f;
  float_t wPx = 256.f;
  float_t hPx = 40.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  CRender render{
    .textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    },
    .size = Vec2f{ 1.3125f, 0.1875f },
    .pos = Vec2f{ 0.f, 0.68375 },
    .zIndex = 2
  };

  m_sysRender.addEntity(id, render);
}

void GameImpl::constructFakeSoil()
{
  for (float_t x = 0.f; x <= 1.25; x += 0.0625) {
    auto id = System::nextId();

    float_t offsetXpx = 384.f;
    float_t offsetYpx = 0.f;
    float_t wPx = 16.f;
    float_t hPx = 16.f;
    float_t w = wPx / ATLAS_WIDTH_PX;
    float_t h = hPx / ATLAS_HEIGHT_PX;

    CRender render{
      .textureRect = Rectf{
        .x = offsetXpx / ATLAS_WIDTH_PX,
        .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
        .w = w,
        .h = h
      },
      .size = Vec2f{ 0.0625, 0.0625f },
      .pos = Vec2f{ x, 0.6875f },
      .zIndex = 1
    };

    m_sysRender.addEntity(id, render);
  }
}

void GameImpl::constructSoil()
{
  // TODO
}

} // namespace

GamePtr createGame(SysBehaviour& sysBehaviour, SysGrid& sysGrid, SysRender& sysRender,
  EventSystem& eventSystem, const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(sysBehaviour, sysGrid, sysRender, eventSystem, fileSystem,
    logger);
}
