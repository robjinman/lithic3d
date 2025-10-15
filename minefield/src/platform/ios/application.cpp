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

PlatformPathsPtr createPlatformPaths(const std::filesystem::path& bundlePath);
FileSystemPtr createDefaultFileSystem(const PlatformPaths& platformPaths);

namespace
{

class ApplicationImpl : public Application
{
  public:
    ApplicationImpl(const char* bundlePath, WindowDelegatePtr windowDelegate);

    void onViewResize() override;
    bool update() override;
    void onTouchBegin(float x, float y) override;
    void onTouchMove(float x, float y) override;
    void onTouchEnd(float x, float y) override;

  private:
    WindowDelegatePtr m_windowDelegate;
    LoggerPtr m_logger;
    PlatformPathsPtr m_platformPaths;
    FileSystemPtr m_fileSystem;
    AudioSystemPtr m_audioSystem;
    render::RendererPtr m_renderer;
    GamePtr m_game;
    Vec2i m_viewportSize;
};

ApplicationImpl::ApplicationImpl(const char* bundlePath, WindowDelegatePtr windowDelegate)
  : m_windowDelegate(std::move(windowDelegate))
{
  m_logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  m_platformPaths = createPlatformPaths(bundlePath);
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

  return m_game->update();
}

void ApplicationImpl::onTouchBegin(float x, float y)
{
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

ApplicationPtr createApplication(const char* bundlePath, WindowDelegatePtr windowDelegate)
{
  return std::make_unique<ApplicationImpl>(bundlePath, std::move(windowDelegate));
}
