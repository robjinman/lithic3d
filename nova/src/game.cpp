#include "game.hpp"
#include "game_events.hpp"
#include "file_system.hpp"
#include "time.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "scene_builder.hpp"
#include "audio_system.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_ui.hpp"
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
    GameImpl(ComponentStore& componentStore, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
      SysRender& sysRender, SysAnimation& sysAnimation, EventSystem& eventSystem,
      AudioSystem& audioSystem, const FileSystem& fileSystem, Logger& logger);

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
    EventSystem& m_eventSystem;
    AudioSystem& m_audioSystem;
    ComponentStore& m_componentStore;
    SysRender& m_sysRender;
    SysGrid& m_sysGrid;
    SysBehaviour& m_sysBehaviour;
    SysAnimation& m_sysAnimation;
    SceneBuilderPtr m_sceneBuilder;
    InputState m_inputState;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    size_t m_currentTick = 0;
    double m_measuredTickRate = 0;
    EntityId m_playerId = 0;
    std::vector<EntityId> m_pendingDeletion;
    GameState m_gameState;

    void measureTickRate();
    void processKeyboardInput();
    void processMouseInput();
    void playSoundForEvent(const GameEvent& event);
    void onPlayerDeath();
    void restartGame();
};

GameImpl::GameImpl(ComponentStore& componentStore, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
  SysRender& sysRender, SysAnimation& sysAnimation, EventSystem& eventSystem,
  AudioSystem& audioSystem, const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_eventSystem(eventSystem)
  , m_audioSystem(audioSystem)
  , m_componentStore(componentStore)
  , m_sysRender(sysRender)
  , m_sysGrid(sysGrid)
  , m_sysBehaviour(sysBehaviour)
  , m_sysAnimation(sysAnimation)
{
  static auto strGame = hashString("game");
  static auto strUi = hashString("ui");

  m_sceneBuilder = createSceneBuilder(m_eventSystem, m_componentStore, m_sysAnimation,
    m_sysBehaviour, m_sysGrid, m_sysRender);

  m_eventSystem.listen(strGame, [&](const Event& e) {
    auto& gameEvent = dynamic_cast<const GameEvent&>(e);

    playSoundForEvent(gameEvent);

    m_sysRender.processEvent(gameEvent);
    m_sysGrid.processEvent(gameEvent);
    m_sysBehaviour.processEvent(gameEvent);
    m_sysAnimation.processEvent(gameEvent);

    if (gameEvent.name == g_strPlayerDeath) {
      onPlayerDeath();
    }

    if (gameEvent.name == g_strRequestDeletion) {
      auto& event = dynamic_cast<const ERequestDeletion&>(gameEvent);
      m_pendingDeletion.push_back(event.entityId);
    }
  });

  m_eventSystem.listen(strUi, [&](const Event& e) {
    auto& uiEvent = dynamic_cast<const UiEvent&>(e);

    // TODO
  });

  restartGame(); // TODO: Start on main menu
}

void GameImpl::restartGame()
{
  m_eventSystem.processEvents();

  auto entities = m_sceneBuilder->entities();
  for (auto id : entities) {
    DBG_LOG(m_logger, STR("Deleting entity " << id));

    m_sysRender.removeEntity(id);
    m_sysGrid.removeEntity(id);
    m_sysBehaviour.removeEntity(id);
    m_sysAnimation.removeEntity(id);

    m_componentStore.remove(id);
  }

  m_playerId = m_sceneBuilder->buildScene();
  m_gameState = GameState::Playing;
}

void GameImpl::onPlayerDeath()
{
  m_gameState = GameState::Dead;

  // TODO: Display text
}

void GameImpl::playSoundForEvent(const GameEvent& event)
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

  switch (m_gameState) {
    case GameState::Playing: {
      if (m_sysAnimation.hasAnimationPlaying(m_playerId)) {
        return;
      }

      if (m_inputState.left) {
        if (m_sysGrid.tryMove(m_playerId, -1, 0)) {
          m_sysAnimation.playAnimation(m_playerId, strMoveLeft);
        }
      }
      else if (m_inputState.right) {
        if (m_sysGrid.tryMove(m_playerId, 1, 0)) {
          m_sysAnimation.playAnimation(m_playerId, strMoveRight);
        }
      }
      else if (m_inputState.up) {
        if (m_sysGrid.tryMove(m_playerId, 0, 1)) {
          m_sysAnimation.playAnimation(m_playerId, strMoveUp);
        }
      }
      else if (m_inputState.down) {
        if (m_sysGrid.tryMove(m_playerId, 0, -1)) {
          m_sysAnimation.playAnimation(m_playerId, strMoveDown);
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

  m_sysBehaviour.update(m_currentTick);
  m_sysGrid.update(m_currentTick);
  m_sysRender.update(m_currentTick);
  m_sysAnimation.update(m_currentTick);

  m_eventSystem.processEvents();

  for (auto id : m_pendingDeletion) {
    DBG_LOG(m_logger, STR("Deleting entity " << id));

    m_sysRender.removeEntity(id);
    m_sysGrid.removeEntity(id);
    m_sysBehaviour.removeEntity(id);
    m_sysAnimation.removeEntity(id);

    m_componentStore.remove(id);
  }
  m_pendingDeletion.clear();
}

} // namespace

GamePtr createGame(ComponentStore& componentStore, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
  SysRender& sysRender, SysAnimation& sysAnimation, EventSystem& eventSystem,
  AudioSystem& audioSystem, const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(componentStore, sysBehaviour, sysGrid, sysRender, sysAnimation,
    eventSystem, audioSystem, fileSystem, logger);
}
