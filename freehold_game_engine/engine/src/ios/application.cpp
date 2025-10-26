#include "application.hpp"
#include <fge/logger.hpp>
#include <fge/game.hpp>
#include <fge/engine.hpp>
#include <fge/renderer.hpp>
#include <fge/time.hpp>
#include <fge/window_delegate.hpp>
#include <fge/file_system.hpp>
#include <fge/platform_paths.hpp>
#include <fge/audio_system.hpp>
#include <iostream>

namespace fs = std::filesystem;

namespace fge
{

PlatformPathsPtr createPlatformPaths(const fs::path& bundlePath, const fs::path& appSupportPath);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

namespace
{

class ApplicationImpl : public Application
{
  public:
    ApplicationImpl(const char* bundlePath, const char* appSupportPath,
      WindowDelegatePtr windowDelegate);

    void onViewResize(float w, float h) override;
    bool update() override;
    void onTouchBegin(float x, float y) override;
    void onTouchMove(float x, float y) override;
    void onTouchEnd(float x, float y) override;
    void onButtonDown(GamepadButton button) override;
    void onButtonUp(GamepadButton button) override;
    void onLeftStickMove(float x, float y) override;
    void onRightStickMove(float x, float y) override;
    void hideMobileControls() override;

  private:
    EnginePtr m_engine;
    GamePtr m_game;
    Vec2i m_screenSize;
    Vec2f m_leftStickDelta;
    Vec2f m_rightStickDelta;
};

ApplicationImpl::ApplicationImpl(const char* bundlePath, const char* appSupportPath,
  WindowDelegatePtr windowDelegate)
{
  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto platformPaths = createPlatformPaths(bundlePath, appSupportPath);
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));
  auto audioSystem = createAudioSystem(*fileSystem);

  render::ScreenMargins margins{
    .left = 50,
    .bottom = 50
  };
  auto renderer = createRenderer(std::move(windowDelegate), *fileSystem, *logger, margins);

  m_engine = createEngine(std::move(renderer), std::move(audioSystem), std::move(fileSystem),
    std::move(logger));

  m_game = createGame(*m_engine);
}

void ApplicationImpl::onViewResize(float w, float h)
{
  m_screenSize = { static_cast<int>(w), static_cast<int>(h) };
  m_game->onWindowResize(w, h);
}

bool ApplicationImpl::update()
{
  auto screenSize = m_engine->renderer().getScreenSize();
  if (screenSize != m_screenSize) {
    m_game->onWindowResize(screenSize[0], screenSize[1]);
    m_screenSize = screenSize;
  }

  m_game->onLeftStickMove(m_leftStickDelta);
  m_game->onRightStickMove(m_rightStickDelta);

  return m_game->update();
}

void ApplicationImpl::hideMobileControls()
{
  m_game->hideMobileControls();
}

void ApplicationImpl::onButtonDown(GamepadButton button)
{
  m_game->onButtonDown(button);
}

void ApplicationImpl::onButtonUp(GamepadButton button)
{
  m_game->onButtonUp(button);
}

void ApplicationImpl::onLeftStickMove(float x, float y)
{
  m_leftStickDelta = { x, y };
}

void ApplicationImpl::onRightStickMove(float x, float y)
{
  m_rightStickDelta = { x, y };
}

void ApplicationImpl::onTouchBegin(float x, float y)
{
  static auto aspect = m_game->gameViewportAspectRatio();

  auto viewport = m_engine->renderer().getViewportSize();
  float screenAspect = static_cast<float>(viewport[0]) / viewport[1];

  float xNorm = x / viewport[1];
  float x0 = (screenAspect - aspect) / 2.f;
  float x1 = x0 + aspect;

  if (xNorm < x0 || xNorm > x1) {
    m_game->showMobileControls();
  }

  m_game->onMouseMove({ x, y }, {});
  m_game->onMouseButtonDown();
}

void ApplicationImpl::onTouchMove(float x, float y)
{
  m_game->onMouseMove({ x, y }, {});
}

void ApplicationImpl::onTouchEnd(float x, float y)
{
  m_game->onMouseMove({ x, y }, {});
  m_game->onMouseButtonUp();
}

}

ApplicationPtr createApplication(const char* bundlePath, const char* appSupportPath,
  WindowDelegatePtr windowDelegate)
{
  return std::make_unique<ApplicationImpl>(bundlePath, appSupportPath, std::move(windowDelegate));
}

} // namespace fge
