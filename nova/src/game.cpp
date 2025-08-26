#include "game.hpp"
#include "game_events.hpp"
#include "file_system.hpp"
#include "time.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "scene_builder.hpp"
#include "audio_system.hpp"
#include "renderer.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_menu.hpp"
#include "sys_spatial.hpp"
#include "ecs.hpp"
#include "systems.hpp"
#undef max
#undef min

namespace
{

struct InputState
{
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool enter = false;
  bool escape = false;
};

enum class GameState
{
  Playing,
  Paused,
  MainMenu,
  Dead
};

class GameImpl : public Game
{
  public:
    GameImpl(render::Renderer& renderer, AudioSystem& audioSystem, const FileSystem& fileSystem,
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
    AudioSystem& m_audioSystem;
    render::Renderer& m_renderer;
    EventSystemPtr m_eventSystem;
    EcsPtr m_ecs;
    SceneBuilderPtr m_sceneBuilder;
    InputState m_inputState;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    size_t m_currentTick = 0;
    double m_measuredTickRate = 0;
    EntityId m_playerId = 0;
    GameState m_gameState;

    void measureTickRate();
    void processKeyboardInput();
    void processMouseInput();
    void playSoundForEvent(const Event& event);
    void onPlayerDeath();
    void restartGame();
};

GameImpl::GameImpl(render::Renderer& renderer, AudioSystem& audioSystem,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_audioSystem(audioSystem)
  , m_renderer(renderer)
{
  m_eventSystem = createEventSystem(m_logger);
  m_ecs = createEcs(m_logger);

  auto sysAnimation = createSysAnimation(m_ecs->componentStore(), *m_eventSystem, m_logger);
  auto sysBehaviour = createSysBehaviour();
  auto sysGrid = createSysGrid(*m_eventSystem);
  //auto sysMenu = createSysMenu();
  auto sysRender = createSysRender(m_ecs->componentStore(), m_renderer, m_fileSystem, m_logger);
  auto sysSpatial = createSysSpatial(m_ecs->componentStore());

  m_ecs->addSystem(ANIMATION_SYSTEM, std::move(sysAnimation));
  m_ecs->addSystem(BEHAVIOUR_SYSTEM, std::move(sysBehaviour));
  m_ecs->addSystem(GRID_SYSTEM, std::move(sysGrid));
  //m_ecs->addSystem(MENU_SYSTEM, std::move(sysMenu));
  m_ecs->addSystem(RENDER_SYSTEM, std::move(sysRender));
  m_ecs->addSystem(SPATIAL_SYSTEM, std::move(sysSpatial));

  m_sceneBuilder = createSceneBuilder(*m_eventSystem, *m_ecs);

  m_eventSystem->listen([&](const Event& event) {
    playSoundForEvent(event);

    m_ecs->processEvent(event);

    if (event.name == g_strPlayerDeath) {
      onPlayerDeath();
    }
  });

  restartGame(); // TODO: Start on main menu

  m_renderer.start();
}

void GameImpl::restartGame()
{
  // Will delete entities pending deletion
  m_ecs->update(m_currentTick);

  auto entities = m_sceneBuilder->entities();
  for (auto id : entities) {
    m_ecs->removeEntity(id);
  }

  m_eventSystem->dropEvents();

  m_playerId = m_sceneBuilder->buildScene();
  m_logger.info(STR("Player id = " << m_playerId));
  m_gameState = GameState::Playing;
}

void GameImpl::onPlayerDeath()
{
  m_gameState = GameState::Dead;

  // TODO: Display text
}

void GameImpl::playSoundForEvent(const Event& event)
{
  static auto strBang = hashString("bang");

  if (event.name == g_strEntityExplode) {
    m_audioSystem.playSound(strBang);
  }
}

void GameImpl::onKeyDown(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::F: {
      m_logger.info(STR("Simulation tick rate: " << m_measuredTickRate));
      break;
    }
    case KeyboardKey::Left: m_inputState.left = true; break;
    case KeyboardKey::Right: m_inputState.right = true; break;
    case KeyboardKey::Up: m_inputState.up = true; break;
    case KeyboardKey::Down: m_inputState.down = true; break;
    case KeyboardKey::Enter: m_inputState.enter = true; break;
    default: break;
  }
}

void GameImpl::onKeyUp(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::Left: m_inputState.left = false; break;
    case KeyboardKey::Right: m_inputState.right = false; break;
    case KeyboardKey::Up: m_inputState.up = false; break;
    case KeyboardKey::Down: m_inputState.down = false; break;
    case KeyboardKey::Enter: m_inputState.enter = false; break;
    default: break;
  }
}

void GameImpl::onButtonDown(GamepadButton button)
{
}

void GameImpl::onButtonUp(GamepadButton button)
{
}

void GameImpl::onMouseMove(const Vec2f& delta)
{
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
  static auto strMoveLeft = hashString("move_left");
  static auto strMoveRight = hashString("move_right");
  static auto strMoveUp = hashString("move_up");
  static auto strMoveDown = hashString("move_down");

  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs->system(ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs->system(GRID_SYSTEM));

  switch (m_gameState) {
    case GameState::Playing: {
      if (sysAnimation.hasAnimationPlaying(m_playerId)) {
        return;
      }

      if (m_inputState.left) {
        if (sysGrid.tryMove(m_playerId, -1, 0)) {
          sysAnimation.playAnimation(m_playerId, strMoveLeft);
        }
      }
      else if (m_inputState.right) {
        if (sysGrid.tryMove(m_playerId, 1, 0)) {
          sysAnimation.playAnimation(m_playerId, strMoveRight);
        }
      }
      else if (m_inputState.up) {
        if (sysGrid.tryMove(m_playerId, 0, 1)) {
          sysAnimation.playAnimation(m_playerId, strMoveUp);
        }
      }
      else if (m_inputState.down) {
        if (sysGrid.tryMove(m_playerId, 0, -1)) {
          sysAnimation.playAnimation(m_playerId, strMoveDown);
        }
      }

      break;
    }
    case GameState::Dead: {
      if (m_inputState.enter) {
        restartGame();
      }
      break;
    }
    default: break;
  }
}

void GameImpl::processMouseInput()
{

}

void GameImpl::measureTickRate()
{
  ++m_currentTick;
  if (m_currentTick % TICKS_PER_SECOND == 0) {
    m_measuredTickRate = TICKS_PER_SECOND / m_timer.elapsed();
    m_timer.reset();
  }
}

void GameImpl::update()
{
  measureTickRate();
  processKeyboardInput();
  processMouseInput();

  m_ecs->update(m_currentTick);
  m_eventSystem->processEvents();
}

} // namespace

GamePtr createGame(render::Renderer& renderer, AudioSystem& audioSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(renderer, audioSystem, fileSystem, logger);
}
