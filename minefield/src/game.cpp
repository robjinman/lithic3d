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
#include "b_player.hpp"
#undef max
#undef min

namespace
{

const auto strBang = hashString("bang");
const auto strCollect = hashString("collect");
const auto strEnterPortal = hashString("enter_portal");
const auto strScream = hashString("scream");
const auto strThrow = hashString("throw");
const auto strTick = hashString("tick");

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
    GameImpl(render::Renderer& renderer, AudioSystem& audioSystem, FileSystem& fileSystem,
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
    void onWindowResize(uint32_t w, uint32_t h) override;
    bool update() override;

  private:
    Logger& m_logger;
    FileSystem& m_fileSystem;
    AudioSystem& m_audioSystem;
    render::Renderer& m_renderer;
    GameOptionsManagerPtr m_options;
    EventSystemPtr m_eventSystem;
    MenuSystemPtr m_menuSystem;
    EcsPtr m_ecs;
    SceneBuilderPtr m_sceneBuilder;
    InputState m_inputState;
    Timer m_timer;
    Tick m_currentTick = 0;
    Tick m_timeSinceStart = 0;
    double m_measuredTickRate = 0;
    Scene m_scene;
    GameState m_gameState;
    bool m_shouldExit = false;
    bool m_throwingMode = false;
    EntityId m_stickId = NULL_ENTITY;
    float m_sfxVolume = 0.75f;
    float m_musicVolume = 0.75f;
    Vec2i m_viewportOffset;

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
    void positionThrowingIndicator(const Vec2f& pos);
    void throwStick(float_t x, float_t y);
    void adjustVolume();
    void setViewport(uint32_t w, uint32_t h);
};

GameImpl::GameImpl(render::Renderer& renderer, AudioSystem& audioSystem, FileSystem& fileSystem,
  Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_audioSystem(audioSystem)
  , m_renderer(renderer)
{
  m_audioSystem.addMusic("sounds/music.ogg");
  m_audioSystem.addSound(strBang, "sounds/bang.wav");
  m_audioSystem.addSound(strCollect, "sounds/collect.wav");
  m_audioSystem.addSound(strEnterPortal, "sounds/enter_portal.wav");
  m_audioSystem.addSound(strScream, "sounds/scream.wav");
  m_audioSystem.addSound(strThrow, "sounds/throw.wav");
  m_audioSystem.addSound(strTick, "sounds/tick.wav");

  m_options = createGameOptionsManager(m_fileSystem, m_logger);

  m_eventSystem = createEventSystem(m_logger);
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

  m_menuSystem = createMenuSystem(*m_ecs, *m_eventSystem, *m_options, m_logger);
  m_sceneBuilder = createSceneBuilder(*m_eventSystem, *m_ecs, *m_options);

  m_eventSystem->listen([this](const Event& event) { handleEvent(event); });

  m_renderer.start();

  dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM)).setEnabled(m_menuSystem->root(), true);
  m_menuSystem->showMainMenu();

  m_gameState = GameState::MainMenu;

  m_audioSystem.playMusic();

  auto viewport = m_renderer.getViewportSize();
  setViewport(viewport[0], viewport[1]);
}

void GameImpl::setViewport(uint32_t w, uint32_t h)
{
  float_t aspect = 630.f / 480.f;
  float gameAreaWidth = aspect * h;
  int x = static_cast<int>(0.5f * (static_cast<float>(w) - gameAreaWidth));
  Recti viewport{
    .x = x,
    .y = 0,
    .w = static_cast<int>(gameAreaWidth),
    .h = static_cast<int>(h)
  };
  dynamic_cast<SysRender&>(m_ecs->system(RENDER_SYSTEM)).addViewport(MAIN_VIEWPORT, viewport);

  m_viewportOffset = { viewport.x, viewport.y };
}

void GameImpl::onWindowResize(uint32_t w, uint32_t h)
{
  setViewport(w, h);
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
  float_t x = GRID_W * GRID_CELL_W * 0.5f;
  float_t y = GRID_H * GRID_CELL_H * 0.5f;
  m_inputState.mousePos = { x, y };
  positionThrowingIndicator({ x, y });
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
  // Will delete entities pending deletion
  m_ecs->update(m_currentTick, m_inputState);

  // TODO: Delete children before parents
  auto entities = m_sceneBuilder->entities();
  for (auto id : entities) {
    m_ecs->removeEntity(id);
  }
  m_ecs->removeEntity(m_scene.worldRoot);

  m_eventSystem->dropEvents();

  m_audioSystem.stopAllSounds();
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

  toggleThrowingMode(false);
}

void GameImpl::onPlayerVictorious()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));

  auto level = m_menuSystem->difficultyLevel();
  uint32_t seconds = static_cast<uint32_t>(m_timeSinceStart / TICKS_PER_SECOND);
  uint32_t bestTime = m_options->getOptionsForLevel(level).bestTime;
  if (seconds < bestTime) {
    m_options->setBestTime(level, seconds);
  }

  destroyCurrentGame();
  sysSpatial.setEnabled(m_menuSystem->root(), true);
  m_menuSystem->showMainMenu();
  m_gameState = GameState::MainMenu;
}

void GameImpl::playSoundForEvent(const Event& event)
{
  if (event.name == g_strEntityExplode) {
    m_audioSystem.playSound(strBang);
  }
  else if (event.name == g_strEnterPortal) {
    m_audioSystem.playSound(strEnterPortal);
  }
  else if (event.name == g_strTenSecondsRemaining) {
    m_audioSystem.playSound(strTick);
  }
  else if (event.name == g_strThrow) {
    m_audioSystem.playSound(strThrow);
  }
  else if (event.name == g_strTimeout || event.name == g_strWandererAttack) {
    m_audioSystem.playSound(strScream);
  }
  else if (event.name == g_strItemCollect) {
    auto& e = dynamic_cast<const EItemCollect&>(event);
    if (e.value > 0) {
      m_audioSystem.playSound(strCollect);
    }
  }
}

void GameImpl::onKeyDown(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::F: {
      m_logger.info(STR("Simulation tick rate: " << m_measuredTickRate));
      break;
    }
    case KeyboardKey::Enter: {
      if (m_gameState == GameState::Playing) {
        if (m_throwingMode) {
          float_t x = m_inputState.mousePos[0];
          float_t y = m_inputState.mousePos[1];
          throwStick(x, y);
        }
      }
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
  //m_inputState.gamepadButtonsPressed.insert(button);

  switch (button) {
    case GamepadButton::A: return onKeyDown(KeyboardKey::Enter);
    case GamepadButton::B: return onKeyDown(KeyboardKey::Escape);
    case GamepadButton::Left: return onKeyDown(KeyboardKey::Left);
    case GamepadButton::Right: return onKeyDown(KeyboardKey::Right);
    case GamepadButton::Up: return onKeyDown(KeyboardKey::Up);
    case GamepadButton::Down: return onKeyDown(KeyboardKey::Down);
    default: break;
  }
}

void GameImpl::onButtonUp(GamepadButton button)
{
  //m_inputState.gamepadButtonsPressed.erase(button);

  switch (button) {
    case GamepadButton::A: return onKeyUp(KeyboardKey::Enter);
    case GamepadButton::B: return onKeyUp(KeyboardKey::Escape);
    case GamepadButton::Left: return onKeyUp(KeyboardKey::Left);
    case GamepadButton::Right: return onKeyUp(KeyboardKey::Right);
    case GamepadButton::Up: return onKeyUp(KeyboardKey::Up);
    case GamepadButton::Down: return onKeyUp(KeyboardKey::Down);
    default: break;
  }
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
  int H = m_renderer.getViewportSize()[1];
  m_inputState.mousePos[0] = (pos[0] - m_viewportOffset[0]) / H;
  m_inputState.mousePos[1] = 1.f - (pos[1] - m_viewportOffset[1]) / H;
}

void GameImpl::onLeftStickMove(const Vec2f& delta)
{
}

void GameImpl::onRightStickMove(const Vec2f& delta)
{
  const float_t threshold = 0.1;
  const float_t sensitivity = 0.01;

  if (delta.squareMagnitude() >= threshold) {
    m_inputState.mousePos += delta * Vec2f{ sensitivity, -sensitivity };
  }
}

void GameImpl::processKeyboardInput()
{
  static auto strPlayer = hashString("player");

  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs->system(BEHAVIOUR_SYSTEM));

  switch (m_gameState) {
    case GameState::Playing: {
      auto& player = dynamic_cast<BPlayer&>(sysBehaviour.getBehaviour(m_scene.player, strPlayer));

      if (m_inputState.keysPressed.contains(KeyboardKey::Left)) {
        if (player.moveLeft()) {
          toggleThrowingMode(false);
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Right)) {
        if (player.moveRight()) {
          toggleThrowingMode(false);
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Up)) {
        if (player.moveUp()) {
          toggleThrowingMode(false);
        }
      }
      else if (m_inputState.keysPressed.contains(KeyboardKey::Down)) {
        if (player.moveDown()) {
          toggleThrowingMode(false);
        }
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
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs->system(ANIMATION_SYSTEM));

  if (sysAnimation.hasAnimationPlaying(m_stickId)) {
    return;
  }

  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs->system(GRID_SYSTEM));

  Vec2i dest{ static_cast<int>(x / GRID_CELL_W), static_cast<int>(y / GRID_CELL_H) };

  if (sysGrid.isInRange(dest[0], dest[1])) {
    m_eventSystem->raiseEvent(EThrow{m_stickId, dest[0], dest[1]});
    toggleThrowingMode(false);
  }
}

void GameImpl::positionThrowingIndicator(const Vec2f& pos)
{
  auto& t = m_ecs->componentStore().component<CLocalTransform>(m_scene.throwingModeIndicator);

  t.transform.set(0, 3, pos[0] - 0.025f);
  t.transform.set(1, 3, pos[1] - 0.025f);
}

void GameImpl::processMouseInput()
{
  if (m_gameState == GameState::Playing) {
    if (m_throwingMode) {
      float_t x = m_inputState.mousePos[0];
      float_t y = m_inputState.mousePos[1];

      positionThrowingIndicator({ x, y });

      if (m_inputState.mouseButtonsPressed.contains(MouseButton::Left)) {
        throwStick(x, y);
      }
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
      GameOptions options = m_options->getOptionsForLevel(m_menuSystem->difficultyLevel());

      const uint32_t gameDuration = options.timeAvailable;
      uint32_t secondsElapsed = static_cast<float>(m_timeSinceStart) / TICKS_PER_SECOND;
      uint32_t timeRemaining = gameDuration - secondsElapsed;

      if (secondsElapsed <= gameDuration) {
        m_eventSystem->raiseEvent(ETimerTick{timeRemaining});
      }

      if (secondsElapsed >= gameDuration) {
        m_eventSystem->raiseEvent(ETimeout{});
      }
    }
  }
}

void GameImpl::adjustVolume()
{
  if (m_menuSystem->musicVolume() != m_musicVolume) {
    m_musicVolume = m_menuSystem->musicVolume();
    m_audioSystem.setMusicVolume(m_musicVolume);
  }

  if (m_menuSystem->sfxVolume() != m_sfxVolume) {
    m_sfxVolume = m_menuSystem->sfxVolume();
    m_audioSystem.setSoundVolume(m_sfxVolume);
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
  m_menuSystem->update();

  adjustVolume();

  return !m_shouldExit;
}

} // namespace

GamePtr createGame(render::Renderer& renderer, AudioSystem& audioSystem, FileSystem& fileSystem,
  Logger& logger)
{
  return std::make_unique<GameImpl>(renderer, audioSystem, fileSystem, logger);
}
