#include "lithic3d/engine.hpp"
#include "lithic3d/renderer.hpp"
#include "lithic3d/audio_system.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/events.hpp"
#include "lithic3d/systems.hpp"
#include "lithic3d/sys_animation_2d.hpp"
#include "lithic3d/sys_behaviour.hpp"
#include "lithic3d/sys_render_2d.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_collision.hpp"
#include "lithic3d/sys_ui.hpp"
#include "lithic3d/model_loader.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/time.hpp"
#include "lithic3d/trace.hpp"

namespace lithic3d
{
namespace
{

class EngineImpl : public Engine
{
  public:
    EngineImpl(ResourceManagerPtr resourceManager, render::RendererPtr renderer,
      AudioSystemPtr audioSystem, FileSystemPtr fileSystem, LoggerPtr logger);

    void setClearColour(const Vec4f& colour) override;
    void update(const InputState& inputState) override;
    void onWindowResize(uint32_t w, uint32_t h) override;
    Tick currentTick() const override;
    float measuredTickRate() const override;

    Logger& logger() override;
    FileSystem& fileSystem() override;
    EventSystem& eventSystem() override;
    render::Renderer& renderer() override;
    AudioSystem& audioSystem() override;
    Ecs& ecs() override;
    ModelLoader& modelLoader() override;
    ResourceManager& resourceManager() override;
    RenderResourceLoader& renderResourceLoader() override;

    ~EngineImpl() override;

  private:
    LoggerPtr m_logger;
    ResourceManagerPtr m_resourceManager;
    render::RendererPtr m_renderer;
    AudioSystemPtr m_audioSystem;
    FileSystemPtr m_fileSystem;
    RenderResourceLoaderPtr m_renderResourceLoader;
    ModelLoaderPtr m_modelLoader;
    EventSystemPtr m_eventSystem;
    EcsPtr m_ecs;
    Vec4f m_clearColour = { 0.f, 0.f, 0.f, 1.f };
    Tick m_currentTick = 0;
    float m_measuredTickRate = 0.f;
    Timer m_timer;

    void measureTickRate();
    void handleEvent(const Event& event);
};

EngineImpl::EngineImpl(ResourceManagerPtr resourceManager, render::RendererPtr renderer,
  AudioSystemPtr audioSystem, FileSystemPtr fileSystem, LoggerPtr logger)
  : m_logger(std::move(logger))
  , m_resourceManager(std::move(resourceManager))
  , m_renderer(std::move(renderer))
  , m_audioSystem(std::move(audioSystem))
  , m_fileSystem(std::move(fileSystem))
{
  m_renderResourceLoader = createRenderResourceLoader(*m_resourceManager, *m_fileSystem,
    *m_renderer, *m_logger);
  m_eventSystem = createEventSystem(*m_logger);
  m_ecs = createEcs(*m_logger);

  m_renderer->start();

  m_modelLoader = createModelLoader(*m_renderResourceLoader, *m_resourceManager, *m_fileSystem,
    *m_logger);

  auto sysCollision = createSysCollision(*m_ecs, *m_eventSystem, *m_logger);
  auto sysRender2d = createSysRender2d(m_ecs->componentStore(), *m_renderer,
    *m_renderResourceLoader, *m_logger);
  auto sysRender3d = createSysRender3d(*m_ecs, *m_modelLoader, *m_renderer, *m_logger);
  auto sysSpatial = createSysSpatial(*m_ecs, *m_eventSystem);
  auto sysAnimation2d = createSysAnimation2d(m_ecs->componentStore(), *m_logger);
  auto sysBehaviour = createSysBehaviour(m_ecs->componentStore());
  auto sysUi = createSysUi(*m_ecs, *m_logger);

  m_ecs->addSystem(Systems::Collision, std::move(sysCollision));
  m_ecs->addSystem(Systems::Render2d, std::move(sysRender2d));
  m_ecs->addSystem(Systems::Render3d, std::move(sysRender3d));
  m_ecs->addSystem(Systems::Spatial, std::move(sysSpatial));
  m_ecs->addSystem(Systems::Animation2d, std::move(sysAnimation2d));
  m_ecs->addSystem(Systems::Behaviour, std::move(sysBehaviour));
  m_ecs->addSystem(Systems::Ui, std::move(sysUi));

  m_eventSystem->listen([this](const Event& event) { handleEvent(event); });
}

void EngineImpl::handleEvent(const Event& event)
{
  m_ecs->processEvent(event);
}

void EngineImpl::setClearColour(const Vec4f& colour)
{
  m_clearColour = colour;
}

Tick EngineImpl::currentTick() const
{
  return m_currentTick;
}

float EngineImpl::measuredTickRate() const
{
  return m_measuredTickRate;
}

void EngineImpl::measureTickRate()
{
  if (m_currentTick % 10 == 0) {
    m_measuredTickRate = 10.f / m_timer.elapsed();
    m_timer.reset();
  }
}

void EngineImpl::update(const InputState& inputState)
{
  try {
    m_renderer->beginFrame(m_clearColour);

    m_ecs->update(m_currentTick, inputState);
    measureTickRate();
    ++m_currentTick;

    m_renderer->endFrame();
    m_renderer->checkError();
  }
  catch (const std::exception& e) {
    EXCEPTION(STR("Error rendering scene; " << e.what()));
  }

  m_eventSystem->processEvents();
}

void EngineImpl::onWindowResize(uint32_t w, uint32_t h)
{
  m_renderer->onResize();
  m_eventSystem->raiseEvent(EWindowResize{w, h});
}

Logger& EngineImpl::logger()
{
  return *m_logger;
}

FileSystem& EngineImpl::fileSystem()
{
  return *m_fileSystem;
}

EventSystem& EngineImpl::eventSystem()
{
  return *m_eventSystem;
}

render::Renderer& EngineImpl::renderer()
{
  return *m_renderer;
}

AudioSystem& EngineImpl::audioSystem()
{
  return *m_audioSystem;
}

Ecs& EngineImpl::ecs()
{
  return *m_ecs;
}

ModelLoader& EngineImpl::modelLoader()
{
  return *m_modelLoader;
}

ResourceManager& EngineImpl::resourceManager()
{
  return *m_resourceManager;
}

RenderResourceLoader& EngineImpl::renderResourceLoader()
{
  return *m_renderResourceLoader;
}

EngineImpl::~EngineImpl()
{
  DBG_TRACE(*m_logger);

  m_resourceManager->waitAll();
}

} // namespace

EnginePtr createEngine(ResourceManagerPtr resourceManager, render::RendererPtr renderer,
  AudioSystemPtr audioSystem, FileSystemPtr fileSystem, LoggerPtr logger)
{
  return std::make_unique<EngineImpl>(std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));
}

} // namespace lithic3d
