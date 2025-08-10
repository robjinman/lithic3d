#include "game.hpp"
#include "player.hpp"
#include "spatial_system.hpp"
#include "render_system.hpp"
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
    GameImpl(SpatialSystem& spatialSystem, RenderSystem& renderSystem, const FileSystem& fileSystem,
      Logger& logger);

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
    SpatialSystem& m_spatialSystem;
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

    void constructPlayer();
    void constructSky();
    void constructTrees();
    void constructFakeSoil();
};

GameImpl::GameImpl(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_spatialSystem(spatialSystem)
  , m_renderSystem(renderSystem)
{
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

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.f, 0.75f, -10.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 416.f;
  float_t wPx = 128.f;
  float_t hPx = 32.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->zIndex = 0;
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 1.3125f, 0.25f };

  m_renderSystem.addComponent(std::move(render));
}

void GameImpl::constructTrees()
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.f, 0.68375, -8.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 352.f;
  float_t wPx = 256.f;
  float_t hPx = 40.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->zIndex = 2;
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 1.3125f, 0.1875f };

  m_renderSystem.addComponent(std::move(render));
}

void GameImpl::constructFakeSoil()
{
  for (float_t x = 0.f; x <= 1.25; x += 0.0625) {
    auto id = System::nextId();

    auto spatial = std::make_unique<CSpatial>(id);
    spatial->transform = translationMatrix4x4(Vec3f{ x, 0.6875f, -9.f });
    m_spatialSystem.addComponent(std::move(spatial));

    float_t offsetXpx = 384.f;
    float_t offsetYpx = 0.f;
    float_t wPx = 16.f;
    float_t hPx = 16.f;
    float_t w = wPx / ATLAS_WIDTH_PX;
    float_t h = hPx / ATLAS_HEIGHT_PX;

    auto render = std::make_unique<CRender>(id);
    render->zIndex = 1;
    render->textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    };
    render->size = Vec2f{ 0.0625, 0.0625f };

    m_renderSystem.addComponent(std::move(render));
  }
}

void GameImpl::constructPlayer()
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.5f, 0.2f, 0.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 384.f;
  float_t offsetYpx = 256.f;
  float_t wPx = 32.f;
  float_t hPx = 48.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 0.0625f, 0.0625f };

  m_renderSystem.addComponent(std::move(render));

  m_player = std::make_unique<Player>();
}

} // namespace

GamePtr createGame(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(spatialSystem, renderSystem, fileSystem, logger);
}
