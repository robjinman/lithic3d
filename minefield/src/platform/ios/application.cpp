#include "platform/ios/application.h"
#include "logger.hpp"
#include "game.hpp"
#include "renderer.hpp"
#include "time.hpp"
#include "window_delegate.hpp"
#include "file_system.hpp"
#include "platform_paths.hpp"
#include "audio_system.hpp"
#include <iostream>

namespace fs = std::filesystem;

PlatformPathsPtr createPlatformPaths(const fs::path& bundlePath, const fs::path& appSupportPath);
FileSystemPtr createDefaultFileSystem(const PlatformPaths& platformPaths);

namespace
{

class ApplicationImpl : public Application
{
  public:
    ApplicationImpl(const char* bundlePath, const char* appSupportPath,
      WindowDelegatePtr windowDelegate);

    void onViewResize() override;
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
    WindowDelegatePtr m_windowDelegate;
    LoggerPtr m_logger;
    PlatformPathsPtr m_platformPaths;
    FileSystemPtr m_fileSystem;
    AudioSystemPtr m_audioSystem;
    render::RendererPtr m_renderer;
    GamePtr m_game;
    Vec2i m_viewportSize;
    Vec2f m_leftStickDelta;
    Vec2f m_rightStickDelta;
};

ApplicationImpl::ApplicationImpl(const char* bundlePath, const char* appSupportPath,
  WindowDelegatePtr windowDelegate)
  : m_windowDelegate(std::move(windowDelegate))
{
  m_logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  m_platformPaths = createPlatformPaths(bundlePath, appSupportPath);
  m_fileSystem = createDefaultFileSystem(*m_platformPaths);
  m_audioSystem = createAudioSystem(*m_fileSystem);
  m_renderer = createRenderer(*m_fileSystem, *m_windowDelegate, *m_logger);

  m_game = createGame(*m_renderer, *m_audioSystem, *m_fileSystem, *m_logger, false);
  m_game->showMobileControls();
}

void ApplicationImpl::onViewResize()
{
  m_renderer->onResize();
}

bool ApplicationImpl::update()
{
  auto viewportSize = m_renderer->getViewportSize();
  if (viewportSize != m_viewportSize) {
    m_game->onWindowResize(viewportSize[0], viewportSize[1]);
    m_viewportSize = viewportSize;
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
  auto viewport = m_renderer->getViewportSize();
  float screenAspect = static_cast<float>(viewport[0]) / viewport[1];

  float xNorm = x / viewport[1];
  float x0 = (screenAspect - GAME_AREA_ASPECT) / 2.f;
  float x1 = x0 + GAME_AREA_ASPECT;

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
