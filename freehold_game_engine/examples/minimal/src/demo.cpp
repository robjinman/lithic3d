#include <fge/game.hpp>
#include <fge/ecs.hpp>
#include <fge/event_system.hpp>
#include <fge/systems.hpp>
#include <fge/sys_render.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/renderer.hpp>
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
    Demo(fge::render::Renderer& renderer, fge::AudioSystem& audioSystem,
      fge::FileSystem& fileSystem, fge::Logger& logger);

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
    fge::Logger& m_logger;
    fge::render::Renderer& m_renderer;
    fge::AudioSystem& m_audioSystem;
    fge::FileSystem& m_fileSystem;
    fge::EventSystemPtr m_eventSystem;
    fge::EcsPtr m_ecs;
    fge::InputState m_inputState;
    Tick m_currentTick = 0;
};

Demo::Demo(fge::render::Renderer& renderer, fge::AudioSystem& audioSystem,
  fge::FileSystem& fileSystem, fge::Logger& logger)
  : m_logger(logger)
  , m_renderer(renderer)
  , m_audioSystem(audioSystem)
  , m_fileSystem(fileSystem)
{
  m_eventSystem = fge::createEventSystem(m_logger);
  m_ecs = fge::createEcs(m_logger);

  auto sysRender = createSysRender(m_ecs->componentStore(), m_renderer, m_fileSystem, m_logger);
  auto sysSpatial = createSysSpatial(m_ecs->componentStore(), *m_eventSystem);

  sysRender->setClearColour({ 0.f, 0.f, 0.f, 1.f });

  m_ecs->addSystem(fge::RENDER_SYSTEM, std::move(sysRender));
  m_ecs->addSystem(fge::SPATIAL_SYSTEM, std::move(sysSpatial));

  m_renderer.start();
}

float Demo::gameViewportAspectRatio() const
{
  return 1.4;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_logger.info("Key down");
  m_inputState.keysPressed.insert(key);
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_logger.info("Key up");
  m_inputState.keysPressed.erase(key);
}

void Demo::onButtonDown(GamepadButton button)
{
  m_logger.info("Button down");
  // Map to KeyboardKey
}

void Demo::onButtonUp(GamepadButton button)
{
  m_logger.info("Button up");
  // Map to KeyboardKey
}

void Demo::onMouseButtonDown()
{
  m_logger.info("Mouse button down");
  m_inputState.mouseButtonsPressed.insert(fge::MouseButton::Left);
}

void Demo::onMouseButtonUp()
{
  m_logger.info("Mouse button up");
  m_inputState.mouseButtonsPressed.erase(fge::MouseButton::Left);
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_logger.info("Mouse move");
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
}

void Demo::onLeftStickMove(const Vec2f& delta)
{
  m_logger.info("Left stick move");
}

void Demo::onRightStickMove(const Vec2f& delta)
{
  m_logger.info("Right stick move");
}

void Demo::onWindowResize(uint32_t w, uint32_t h)
{
  m_logger.info("Window resize");
}

void Demo::hideMobileControls()
{
  m_logger.info("Hide mobile controls");
}

void Demo::showMobileControls()
{
  m_logger.info("Show mobile controls");
}

bool Demo::update()
{
  m_ecs->update(m_currentTick++, m_inputState);
  m_eventSystem->processEvents();

  return true;
}

} // namespace

fge::GamePtr createGame(fge::render::Renderer& renderer, fge::AudioSystem& audioSystem,
  fge::FileSystem& fileSystem, fge::Logger& logger)
{
  return std::make_unique<Demo>(renderer, audioSystem, fileSystem, logger);
}
