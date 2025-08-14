#include "game.hpp"
#include "sys_render.hpp"
#include "sys_grid.hpp"
#include "sys_behaviour.hpp"
#include "sys_ui.hpp"
#include "file_system.hpp"
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

const float_t ATLAS_WIDTH_PX = 1024.f;
const float_t ATLAS_HEIGHT_PX = 512.f;

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
    std::set<KeyboardKey> m_keysPressed;
    Vec2f m_mouseDelta;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    size_t m_frame = 0;
    double m_measuredFrameRate = 0;

    void measureFrameRate();
    void processKeyboardInput();
    void processMouseInput();

    void constructPlayer();
    void constructSky();
    void constructTrees();
    void constructFakeSoil();
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

  constructPlayer();
  constructSky();
  constructTrees();
  constructFakeSoil();
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

void GameImpl::constructPlayer()
{
  auto id = System::nextId();

  float_t offsetXpx = 384.f;
  float_t offsetYpx = 256.f;
  float_t wPx = 32.f;
  float_t hPx = 48.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  CRender render{
    .textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    },
    .size = Vec2f{ 0.0625f, 0.0625f },
    .pos = Vec2f{ 0.5f, 0.2f },
    .zIndex = 2
  };

  m_sysRender.addEntity(id, render);
}

} // namespace

GamePtr createGame(SysBehaviour& sysBehaviour, SysGrid& sysGrid, SysRender& sysRender,
  EventSystem& eventSystem, const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(sysBehaviour, sysGrid, sysRender, eventSystem, fileSystem,
    logger);
}
