#include "fge/engine.hpp"
#include "fge/renderer.hpp"
#include "fge/audio_system.hpp"
#include "fge/file_system.hpp"
#include "fge/logger.hpp"
#include "fge/ecs.hpp"
#include "fge/event_system.hpp"
#include "fge/systems.hpp"
#include "fge/sys_animation_2d.hpp"
#include "fge/sys_behaviour.hpp"
#include "fge/sys_render_2d.hpp"
#include "fge/sys_render_3d.hpp"
#include "fge/sys_spatial.hpp"
#include "fge/sys_ui.hpp"

namespace fge
{
namespace
{

class EngineImpl : public Engine
{
  public:
    EngineImpl(render::RendererPtr renderer, AudioSystemPtr audioSystem, FileSystemPtr fileSystem,
      LoggerPtr logger);

    Logger& logger() override;
    FileSystem& fileSystem() override;
    EventSystem& eventSystem() override;
    render::Renderer& renderer() override;
    AudioSystem& audioSystem() override;
    Ecs& ecs() override;

  private:
    render::RendererPtr m_renderer;
    AudioSystemPtr m_audioSystem;
    FileSystemPtr m_fileSystem;
    LoggerPtr m_logger;
    EventSystemPtr m_eventSystem;
    EcsPtr m_ecs;
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

  sysRender2d->setClearColour({ 0.f, 0.f, 0.f, 1.f });
  sysRender3d->setClearColour({ 0.f, 0.f, 0.f, 1.f });

  m_ecs->addSystem(RENDER_2D_SYSTEM, std::move(sysRender2d));
  m_ecs->addSystem(RENDER_3D_SYSTEM, std::move(sysRender3d));
  m_ecs->addSystem(SPATIAL_SYSTEM, std::move(sysSpatial));
  m_ecs->addSystem(ANIMATION_2D_SYSTEM, std::move(sysAnimation2d));
  m_ecs->addSystem(BEHAVIOUR_SYSTEM, std::move(sysBehaviour));
  m_ecs->addSystem(UI_SYSTEM, std::move(sysUi));

  //m_renderer->start();
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

} // namespace

EnginePtr createEngine(render::RendererPtr renderer, AudioSystemPtr audioSystem,
  FileSystemPtr fileSystem, LoggerPtr logger)
{
  return std::make_unique<EngineImpl>(std::move(renderer), std::move(audioSystem),
    std::move(fileSystem), std::move(logger));
}

} // namespace fge
