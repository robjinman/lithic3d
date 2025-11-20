#include "lithic3d/engine.hpp"
#include "lithic3d/renderer.hpp"
#include "lithic3d/audio_system.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/event_system.hpp"
#include "lithic3d/systems.hpp"
#include "lithic3d/sys_animation_2d.hpp"
#include "lithic3d/sys_behaviour.hpp"
#include "lithic3d/sys_render_2d.hpp"
#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/sys_ui.hpp"
#include "lithic3d/model_loader.hpp"
#include "lithic3d/time.hpp"

namespace lithic3d
{
namespace
{

class EngineImpl : public Engine
{
  public:
    EngineImpl(render::RendererPtr renderer, AudioSystemPtr audioSystem, FileSystemPtr fileSystem,
      LoggerPtr logger);

    void setClearColour(const Vec4f& colour) override;
    void update(const InputState& inputState) override;
    void onWindowResize() override;
    Tick currentTick() const override;
    float measuredTickRate() const override;

    Logger& logger() override;
    FileSystem& fileSystem() override;
    EventSystem& eventSystem() override;
    render::Renderer& renderer() override;
    AudioSystem& audioSystem() override;
    Ecs& ecs() override;
    ModelLoader& modelLoader() override;

  private:
    render::RendererPtr m_renderer;
    AudioSystemPtr m_audioSystem;
    FileSystemPtr m_fileSystem;
    LoggerPtr m_logger;
    EventSystemPtr m_eventSystem;
    EcsPtr m_ecs;
    ModelLoaderPtr m_modelLoader;
    Vec4f m_clearColour = { 0.f, 0.f, 0.f, 1.f };
    Tick m_currentTick = 0;
    float m_measuredTickRate = 0.f;
    Timer m_timer;

    void measureTickRate();
};

EngineImpl::EngineImpl(render::RendererPtr renderer, AudioSystemPtr audioSystem,
  FileSystemPtr fileSystem, LoggerPtr logger)
  : m_renderer(std::move(renderer))
  , m_audioSystem(std::move(audioSystem))
  , m_fileSystem(std::move(fileSystem))
  , m_logger(std::move(logger))
{
  m_eventSystem = createEventSystem(*m_logger);
  m_ecs = createEcs(*m_logger);

  auto sysRender2d = createSysRender2d(m_ecs->componentStore(), *m_renderer, *m_fileSystem,
    *m_logger);
  auto sysRender3d = createSysRender3d(*m_ecs, *m_renderer, *m_logger);
  auto sysSpatial = createSysSpatial(m_ecs->componentStore(), *m_eventSystem);
  auto sysAnimation2d = createSysAnimation2d(m_ecs->componentStore(), *m_logger);
  auto sysBehaviour = createSysBehaviour(m_ecs->componentStore());
  auto sysUi = createSysUi(*m_ecs, *m_logger);

  m_ecs->addSystem(RENDER_2D_SYSTEM, std::move(sysRender2d));
  m_ecs->addSystem(RENDER_3D_SYSTEM, std::move(sysRender3d));
  m_ecs->addSystem(SPATIAL_SYSTEM, std::move(sysSpatial));
  m_ecs->addSystem(ANIMATION_2D_SYSTEM, std::move(sysAnimation2d));
  m_ecs->addSystem(BEHAVIOUR_SYSTEM, std::move(sysBehaviour));
  m_ecs->addSystem(UI_SYSTEM, std::move(sysUi));

  m_modelLoader = createModelLoader(*m_ecs, *m_fileSystem, *m_logger);

  //m_renderer->start();
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
  catch(const std::exception& e) {
    EXCEPTION(STR("Error rendering scene; " << e.what()));
  }

  m_eventSystem->processEvents();
}

void EngineImpl::onWindowResize()
{
  m_renderer->onResize();
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

} // namespace

EnginePtr createEngine(render::RendererPtr renderer, AudioSystemPtr audioSystem,
  FileSystemPtr fileSystem, LoggerPtr logger)
{
  return std::make_unique<EngineImpl>(std::move(renderer), std::move(audioSystem),
    std::move(fileSystem), std::move(logger));
}

} // namespace lithic3d
