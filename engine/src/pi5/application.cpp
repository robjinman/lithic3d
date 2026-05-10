#include <lithic3d/lithic3d.hpp>
#include <iostream>

namespace lithic3d
{

WindowDelegatePtr createWindowDelegate();
PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

namespace
{

class Application
{
  public:
    Application();

    void run();

    ~Application();

  private:
    GameConfig m_config;
    WindowDelegatePtr m_windowDelegate;
    EnginePtr m_engine;
    GamePtr m_game;

    void cleanUp();
};

Application::Application()
{
  m_config = getGameConfig();

  auto platformPaths = createPlatformPaths(m_config.appShortName, m_config.vendorShortName);
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));
  fillDefaultPaths(*fileSystem, m_config.paths);
  m_windowDelegate = createWindowDelegate();
  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto audioSystem = createAudioSystem(m_config.paths.soundsDir, *logger);
  auto resourceManager = createResourceManager(*logger);
  auto renderer = createRenderer(*m_windowDelegate, *resourceManager, m_config.paths,
    *logger, {});

  logger->info("Compiling shaders...");

  auto manifest = m_config.paths.shaderManifest.read();
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = createEngine(m_config, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));

  try {
    m_game = createGame(*m_engine);
  }
  catch (...) {
    cleanUp();
    throw;
  }
}

void Application::run()
{
  FrameRateLimiter frameRateLimiter{TICKS_PER_SECOND};

  while(true) { // TODO
    if (!m_game->update()) {
      break;
    }

    frameRateLimiter.wait();
  }
}

void Application::cleanUp()
{
}

Application::~Application()
{
  cleanUp();
}

} // namespace
} // namespace lithic3d

int main()
{
  try {
    lithic3d::Application app;
    app.run();
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
