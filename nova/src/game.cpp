#include "game.hpp"
#include "game_events.hpp"
#include "file_system.hpp"
#include "time.hpp"
#include "camera.hpp"
#include "logger.hpp"
#include "scene_builder.hpp"
#include "audio_system.hpp"
#include "renderer.hpp"
#include "menu_system.hpp"
#include "time_service.hpp"
#include "sys_animation.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "sys_ui.hpp"
#include "ecs.hpp"
#include "systems.hpp"
#include "events.hpp"
#include "game_options.hpp"
#undef max
#undef min

namespace
{

enum class GameState
{
  Playing,
  Paused,
  MainMenu,
  Dead,
  DeadPaused
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
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f& pos, const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f& delta) override;
    void onRightStickMove(const Vec2f& delta) override;
    bool update() override;

  private:
    Logger& m_logger;
    const FileSystem& m_fileSystem;
    AudioSystem& m_audioSystem;
    render::Renderer& m_renderer;
    EventSystemPtr m_eventSystem;
    TimeServicePtr m_timeService;
    MenuSystemPtr m_menuSystem;
    EcsPtr m_ecs;
    SceneBuilderPtr m_sceneBuilder;
    InputState m_inputState;
    Vec2f m_leftStickDelta;
    Timer m_timer;
    Tick m_currentTick = 0;
    Tick m_timeSinceStart = 0;
    double m_measuredTickRate = 0;
    Scene m_scene;
    GameState m_gameState;
    bool m_shouldExit = false;
    bool m_throwingMode = false;
    EntityId m_stickId = NULL_ENTITY;

    void measureTickRate();
    void processKeyboardInput();
    void processMouseInput();
    void playSoundForEvent(const Event& event);
    void onPlayerDeath();
    void onPlayerVictorious();
    void destroyCurrentGame();
    void startGame();
    void handleEvent(const Event& event);
    void handleMenuEvent(const Event& event);
    void checkTimeout();
    void toggleThrowingMode(bool on, EntityId stickId = NULL_ENTITY);
    void throwStick(float_t x, float_t y);
};

GameImpl::GameImpl(render::Renderer& renderer, AudioSystem& audioSystem,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_audioSystem(audioSystem)
  , m_renderer(renderer)
{
  m_eventSystem = createEventSystem(m_logger);
  m_timeService = createTimeService();
  m_ecs = createEcs(m_logger);

  auto sysAnimation = createSysAnimation(m_ecs->componentStore(), m_logger);
  auto sysBehaviour = createSysBehaviour(m_ecs->componentStore());
  auto sysGrid = createSysGrid(*m_eventSystem);
  auto sysRender = createSysRender(m_ecs->componentStore(), m_renderer, m_fileSystem, m_logger);
  auto sysSpatial = createSysSpatial(m_ecs->componentStore(), *m_eventSystem);
  auto sysUi = createSysUi(*m_ecs, m_logger);

  m_ecs->addSystem(ANIMATION_SYSTEM, std::move(sysAnimation));
  m_ecs->addSystem(BEHAVIOUR_SYSTEM, std::move(sysBehaviour));
  m_ecs->addSystem(GRID_SYSTEM, std::move(sysGrid));
  m_ecs->addSystem(RENDER_SYSTEM, std::move(sysRender));
  m_ecs->addSystem(SPATIAL_SYSTEM, std::move(sysSpatial));
  m_ecs->addSystem(UI_SYSTEM, std::move(sysUi));

  m_menuSystem = createMenuSystem(*m_ecs, *m_eventSystem, m_logger);
  m_sceneBuilder = createSceneBuilder(*m_eventSystem, *m_ecs, *m_timeService);

  m_eventSystem->listen([this](const Event& event) { handleEvent(event); });

  m_renderer.start();

  dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM)).setEnabled(m_menuSystem->root(), true);
  m_menuSystem->showMainMenu();

  m_gameState = GameState::MainMenu;
}

void GameImpl::handleMenuEvent(const Event& event)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));

  if (event.name == g_strMenuItemActivate) {
    auto& e = dynamic_cast<const EMenuItemActivate&>(event);

    if (e.entityId == m_menuSystem->quitGameBtn()) {
      m_shouldExit = true;
    }
    else if (e.entityId == m_menuSystem->startGameBtn()) {
      startGame();
      sysSpatial.setEnabled(m_menuSystem->root(), false);
      sysSpatial.setEnabled(m_scene.worldRoot, true);
    }
    else if (e.entityId == m_menuSystem->resumeBtn()) {
      sysSpatial.setEnabled(m_menuSystem->root(), false);
      sysSpatial.setEnabled(m_scene.worldRoot, true);
      if (m_gameState == GameState::DeadPaused) {
        m_gameState = GameState::Dead;
      }
      else {
        assert(m_gameState == GameState::Paused);
        m_gameState = GameState::Playing;
      }
    }
    else if (e.entityId == m_menuSystem->quitToMainMenuBtn()) {
      destroyCurrentGame();
      m_menuSystem->showMainMenu();
      m_gameState = GameState::MainMenu;
    }
  }
}

void GameImpl::toggleThrowingMode(bool on, EntityId stickId)
{
  m_throwingMode = on;
  m_ecs->componentStore().component<CRender>(m_scene.throwingModeIndicator).visible = on;
  m_stickId = stickId;
}

void GameImpl::handleEvent(const Event& event)
{
  playSoundForEvent(event);

  m_ecs->processEvent(event);

  if (event.name == g_strPlayerDeath) {
    onPlayerDeath();
  }
  else if (event.name == g_strPlayerVictorious) {
    onPlayerVictorious();
  }
  else if (event.name == g_strMenuItemActivate) {
    handleMenuEvent(event);
  }
  else if (event.name == g_strToggleThrowingMode) {
    auto& e = dynamic_cast<const EToggleThrowingMode&>(event);
    toggleThrowingMode(true, e.stickId);
  }
}

void GameImpl::destroyCurrentGame()
{
  m_timeService->clear();

  // Will delete entities pending deletion
  m_ecs->update(m_currentTick, m_inputState);

  // TODO: Delete children before parents
  auto entities = m_sceneBuilder->entities();
  for (auto id : entities) {
    m_ecs->removeEntity(id);
  }
  m_ecs->removeEntity(m_scene.worldRoot);

  m_eventSystem->dropEvents();
}

void GameImpl::startGame()
{
  m_scene = m_sceneBuilder->buildScene(m_menuSystem->difficultyLevel());
  m_gameState = GameState::Playing;
  m_timeSinceStart = 0;
}

void GameImpl::onPlayerDeath()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));

  m_gameState = GameState::Dead;
  sysSpatial.setEnabled(m_scene.restartGamePrompt, true);
}

void GameImpl::onPlayerVictorious()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));

  destroyCurrentGame();
  sysSpatial.setEnabled(m_scene.worldRoot, false);
  sysSpatial.setEnabled(m_menuSystem->root(), true);
  m_menuSystem->showMainMenu();
  m_gameState = GameState::MainMenu;
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
    case KeyboardKey::Escape: {
      auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));

      if (m_gameState == GameState::Dead || m_gameState == GameState::Playing) {
        sysSpatial.setEnabled(m_menuSystem->root(), true);
        sysSpatial.setEnabled(m_scene.worldRoot, false);
        m_menuSystem->showPauseMenu();
        if (m_gameState == GameState::Dead) {
          m_gameState = GameState::DeadPaused;
        }
        else {
          m_gameState = GameState::Paused;
        }
        return;
      }
      break;
    }
    default: break;
  }

  m_inputState.keysPressed.insert(key);
}

void GameImpl::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void GameImpl::onButtonDown(GamepadButton button)
{
  m_inputState.gamepadButtonsPressed.insert(button);
}

void GameImpl::onButtonUp(GamepadButton button)
{
  m_inputState.gamepadButtonsPressed.erase(button);
}

void GameImpl::onMouseButtonDown()
{
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void GameImpl::onMouseButtonUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void GameImpl::onMouseMove(const Vec2f& pos, const Vec2f&)
{
  int W = 630;
  int H = 480; // TODO (remember fullscreen mode)
  float_t aspect = static_cast<float_t>(W) / H;
  m_inputState.mousePos[0] = pos[0] / H;
  m_inputState.mousePos[1] = 1.f - pos[1] / H;
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
      if (sysAnimation.hasAnimationPlaying(m_scene.player)) {
        return;
      }

      bool moved = false;
      if (m_inputState.keysPressed.contains(KeyboardKey::Left)) {
        if (sysGrid.tryMove(m_scene.player, -1, 0)) {
          sysAnimation.playAnimation(m_scene.player, strMoveLeft);
          moved = true;
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Right)) {
        if (sysGrid.tryMove(m_scene.player, 1, 0)) {
          sysAnimation.playAnimation(m_scene.player, strMoveRight);
          moved = true;
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Up)) {
        if (sysGrid.tryMove(m_scene.player, 0, 1)) {
          sysAnimation.playAnimation(m_scene.player, strMoveUp);
          moved = true;
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Down)) {
        if (sysGrid.tryMove(m_scene.player, 0, -1)) {
          sysAnimation.playAnimation(m_scene.player, strMoveDown);
          moved = true;
        }
      }

      if (moved) {
        auto movedEvent = std::make_unique<EPlayerMove>(sysGrid.entityPos(m_scene.player));
        m_eventSystem->queueEvent(std::move(movedEvent));

        toggleThrowingMode(false);

        auto makeTask = [&sysGrid](EventSystem& eventSystem, EntityId id) {
          return [&eventSystem, &sysGrid, id]() {
            if (sysGrid.hasEntity(id)) {
              auto pos = sysGrid.entityPos(id);
              auto entities = sysGrid.getEntities(pos[0], pos[1]);
              entities.erase(id);

              auto event = std::make_unique<EEntityLandOn>(id, pos, entities);
              eventSystem.queueEvent(std::move(event));
            }
          };
        };

        m_timeService->scheduleTask(15, makeTask(*m_eventSystem, m_scene.player));
      }

      break;
    }
    case GameState::Dead: {
      if (m_inputState.keysPressed.contains(KeyboardKey::Enter)) {
        destroyCurrentGame();
        startGame();
      }
      break;
    }
    default: break;
  }
}

void GameImpl::throwStick(float_t x, float_t y)
{
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs->system(GRID_SYSTEM));

  Vec2i dest{ static_cast<int>(x / GRID_CELL_W), static_cast<int>(y / GRID_CELL_H) };

  if (sysGrid.isInRange(dest[0], dest[1])) {
    m_eventSystem->queueEvent(std::make_unique<EThrow>(m_stickId, dest[0], dest[1]));
    toggleThrowingMode(false);
  }
}

void GameImpl::processMouseInput()
{
  if (m_throwingMode) {
    auto& t = m_ecs->componentStore().component<CLocalTransform>(m_scene.throwingModeIndicator);

    float_t x = m_inputState.mousePos[0];
    float_t y = m_inputState.mousePos[1];

    t.transform.set(0, 3, x - 0.025f);
    t.transform.set(1, 3, y - 0.025f);

    auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs->system(ANIMATION_SYSTEM));

    if (m_inputState.mouseButtonsPressed.contains(MouseButton::Left) &&
      !sysAnimation.hasAnimationPlaying(m_stickId)) {

      throwStick(x, y);
    }
  }
}

void GameImpl::measureTickRate()
{
  ++m_currentTick;
  if (m_currentTick % 10 == 0) {
    m_measuredTickRate = 10.f / m_timer.elapsed();
    m_timer.reset();
  }
}

void GameImpl::checkTimeout()
{
  if (m_gameState == GameState::Playing) {
    ++m_timeSinceStart;
    if (m_timeSinceStart % TICKS_PER_SECOND == 0) {
      GameOptions options = getOptionsForLevel(m_menuSystem->difficultyLevel());

      const uint32_t gameDuration = options.timeAvailable;
      uint32_t secondsElapsed = static_cast<float>(m_timeSinceStart) / TICKS_PER_SECOND;
      uint32_t timeRemaining = gameDuration - secondsElapsed;

      if (secondsElapsed <= gameDuration) {
        m_eventSystem->queueEvent(std::make_unique<ETimerTick>(timeRemaining));
      }

      if (secondsElapsed >= gameDuration) {
        m_eventSystem->queueEvent(std::make_unique<ETimeout>());
      }
    }
  }
}

bool GameImpl::update()
{
  measureTickRate();
  processKeyboardInput();
  processMouseInput();
  checkTimeout();

  m_ecs->update(m_currentTick, m_inputState);
  m_eventSystem->processEvents();
  m_timeService->update();
  m_menuSystem->update();

  return !m_shouldExit;
}

} // namespace

GamePtr createGame(render::Renderer& renderer, AudioSystem& audioSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<GameImpl>(renderer, audioSystem, fileSystem, logger);
}
