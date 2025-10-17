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
#include "mobile_controls.hpp"
#include "platform.hpp"
#ifdef DRM
#include "drm.hpp"
#include "product_activation.hpp"
#endif
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
  ProductActivation,
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
    void hideMobileControls() override;
    void showMobileControls() override;
    bool update() override;

  private:
    Logger& m_logger;
    FileSystem& m_fileSystem;
    AudioSystem& m_audioSystem;
    render::Renderer& m_renderer;
    GameOptionsManagerPtr m_options;
    EventSystemPtr m_eventSystem;
    MenuSystemPtr m_menuSystem;
    MobileControlsPtr m_mobileControls;
    bool m_mobileControlsActive = false;
    EcsPtr m_ecs;
#ifdef DRM
    DrmPtr m_drm;
    ProductActivationPtr m_productActivation;
#endif
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
    Vec2f m_leftStickPrevDelta;
    std::optional<std::function<void()>> m_stateChangeFn;

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
    void throwStick(float x, float y);
    void adjustVolume();
    void setViewport(uint32_t screenW, uint32_t screenH);
    void setMobileControlsViewport(uint32_t screenW, uint32_t screenH);
    bool isInsideGameArea(float x, float y) const;
    Vec2f throwingIndicatorPosition() const;
    Rectf calculateGameArea(uint32_t screenW, uint32_t screenH) const;
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

  sysRender->setClearColour({ 0.f, 0.f, 0.f, 1.f });

  m_ecs->addSystem(ANIMATION_SYSTEM, std::move(sysAnimation));
  m_ecs->addSystem(BEHAVIOUR_SYSTEM, std::move(sysBehaviour));
  m_ecs->addSystem(GRID_SYSTEM, std::move(sysGrid));
  m_ecs->addSystem(RENDER_SYSTEM, std::move(sysRender));
  m_ecs->addSystem(SPATIAL_SYSTEM, std::move(sysSpatial));
  m_ecs->addSystem(UI_SYSTEM, std::move(sysUi));

  m_menuSystem = createMenuSystem(*m_ecs, *m_eventSystem, *m_options, m_logger,
    PLATFORM != Platform::iOS);

  m_sceneBuilder = createSceneBuilder(*m_eventSystem, *m_ecs, *m_options);

  auto viewport = m_renderer.getViewportSize();

  MobileControlsCallbacks callbacks{
    .onLeftButtonPress = [this]() { onKeyDown(KeyboardKey::Left); },
    .onLeftButtonRelease = [this]() { onKeyUp(KeyboardKey::Left); },
    .onRightButtonPress = [this]() { onKeyDown(KeyboardKey::Right); },
    .onRightButtonRelease = [this]() { onKeyUp(KeyboardKey::Right); },
    .onUpButtonPress = [this]() { onKeyDown(KeyboardKey::Up); },
    .onUpButtonRelease = [this]() { onKeyUp(KeyboardKey::Up); },
    .onDownButtonPress = [this]() { onKeyDown(KeyboardKey::Down); },
    .onDownButtonRelease = [this]() { onKeyUp(KeyboardKey::Down); },
    .onActionButtonPress = [this]() { onKeyDown(KeyboardKey::Enter); },
    .onActionButtonRelease = [this]() { onKeyUp(KeyboardKey::Enter); },
    .onEscapeButtonPress = [this]() { onKeyDown(KeyboardKey::Escape); },
    .onEscapeButtonRelease = [this]() { onKeyUp(KeyboardKey::Escape); }
  };
  m_mobileControls = createMobileControls(*m_ecs, *m_eventSystem, callbacks,
    calculateGameArea(viewport[0], viewport[1]));

  m_eventSystem->listen([this](const Event& event) { handleEvent(event); });

  m_renderer.start();

  setViewport(viewport[0], viewport[1]);
  setMobileControlsViewport(viewport[0], viewport[1]);

#ifdef DRM
  m_drm = createDrm(m_fileSystem);

  if (!m_drm->isActivated()) {
    m_productActivation = createProductActivation(*m_ecs, *m_eventSystem, *m_drm, m_logger);
    dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM)).setEnabled(m_productActivation->root(),
      true);
    m_gameState = GameState::ProductActivation;

    return;
  }
#endif

  dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM)).setEnabled(m_menuSystem->root(), true);
  m_menuSystem->showMainMenu();
  m_gameState = GameState::MainMenu;

  m_audioSystem.playMusic();

  if (MOBILE_PLATFORM) {
    showMobileControls();
  }
}

void GameImpl::hideMobileControls()
{
  if (m_mobileControlsActive) {
    m_logger.info("Hiding mobile controls");
    m_mobileControlsActive = false;
    m_mobileControls->hide();
  }
}

void GameImpl::showMobileControls()
{
  if (!m_mobileControlsActive) {
    m_logger.info("Showing mobile controls");
    m_mobileControlsActive = true;
    m_mobileControls->show();
  }
}

Rectf GameImpl::calculateGameArea(uint32_t screenW, uint32_t screenH) const
{
  float screenAspect = static_cast<float>(screenW) / screenH;
  float margin = (screenAspect - GAME_AREA_ASPECT) / 2.f;

  return Rectf{
    .x = margin,
    .y = 0,
    .w = GAME_AREA_ASPECT,
    .h = 1.f
  };
}

void GameImpl::setMobileControlsViewport(uint32_t screenW, uint32_t screenH)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs->system(RENDER_SYSTEM));

  Recti fullScreen{
    .x = 0,
    .y = 0,
    .w = static_cast<int>(screenW),
    .h = static_cast<int>(screenH)
  };

  sysRender.addViewport(MOBILE_CONTROLS_VIEWPORT, fullScreen);
}

void GameImpl::setViewport(uint32_t screenW, uint32_t screenH)
{
  float gameAreaWidth = GAME_AREA_ASPECT * screenH;
  int x = static_cast<int>(0.5f * (static_cast<float>(screenW) - gameAreaWidth));
  Recti viewport{
    .x = x,
    .y = 0,
    .w = static_cast<int>(gameAreaWidth),
    .h = static_cast<int>(screenH)
  };
  dynamic_cast<SysRender&>(m_ecs->system(RENDER_SYSTEM)).addViewport(MAIN_VIEWPORT, viewport);

  m_logger.info(STR("Resetting viewport: screen (w: " << screenW << ", h: " << screenH << "), "
    << "viewport (x: " << viewport.x << ", y: " << viewport.y << ", w: " << viewport.w << ", h: "
    << viewport.h << ")"));

  m_viewportOffset = { viewport.x, viewport.y };
}

void GameImpl::onWindowResize(uint32_t w, uint32_t h)
{
  setViewport(w, h);
  setMobileControlsViewport(w, h);
  m_mobileControls->setGameArea(calculateGameArea(w, h));
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
      m_stateChangeFn = [this]() {
        destroyCurrentGame();
        m_menuSystem->showMainMenu();
        m_gameState = GameState::MainMenu;
      };
    }
  }
}

void GameImpl::toggleThrowingMode(bool on, EntityId stickId)
{
  m_throwingMode = on;
  m_ecs->componentStore().component<CRender>(m_scene.throwingModeIndicator).visible = on;
  if (on) {
    float x = GRID_W * GRID_CELL_W * 0.5f;
    float y = GRID_H * GRID_CELL_H * 0.5f;
    // If mobile controls are active, don't teleport the mouse
    if (!m_mobileControlsActive) {
      m_inputState.mouseX = x;
      m_inputState.mouseY = y;
    }
    positionThrowingIndicator({ x, y });
    m_stickId = stickId;
  }
}

void GameImpl::handleEvent(const Event& event)
{
  playSoundForEvent(event);

  m_ecs->processEvent(event);

  if (event.name == g_strPlayerDeath) {
    onPlayerDeath();
  }
#ifdef DRM
  else if (event.name == g_strProductActivate) {
    auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs->system(SPATIAL_SYSTEM));
    
    sysSpatial.setEnabled(m_productActivation->root(), false);
    sysSpatial.setEnabled(m_menuSystem->root(), true);
    m_menuSystem->showMainMenu();
    m_gameState = GameState::MainMenu;

    m_audioSystem.playMusic();
  }
#endif
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

  m_stateChangeFn = [this, &sysSpatial]() {
    destroyCurrentGame();
    sysSpatial.setEnabled(m_menuSystem->root(), true);
    m_menuSystem->showMainMenu();
    m_gameState = GameState::MainMenu;
  };
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

Vec2f GameImpl::throwingIndicatorPosition() const
{
  const auto& t = m_ecs->componentStore().component<CLocalTransform>(m_scene.throwingModeIndicator);

  return {
    t.transform.at(0, 3) + 0.025f,
    t.transform.at(1, 3) + 0.025f
  };
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
          auto pos = throwingIndicatorPosition();
          throwStick(pos[0], pos[1]);
        }
      }
      break;
    }
    case KeyboardKey::M: {
      if (m_mobileControlsActive) {
        m_mobileControls->hide();
        m_mobileControlsActive = false;
      }
      else {
        m_mobileControls->show();
        m_mobileControlsActive = true;
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

  if (m_gameState == GameState::Dead) {
    m_stateChangeFn = [this]() {
      destroyCurrentGame();
      startGame();
    };

    return;
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

  if (m_gameState == GameState::Dead && button != GamepadButton::B) {
    m_stateChangeFn = [this]() {
      destroyCurrentGame();
      startGame();
    };

    return;
  }

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
  if (m_gameState == GameState::Dead) {
    if (isInsideGameArea(m_inputState.mouseX, m_inputState.mouseY)) {
      m_stateChangeFn = [this]() {
        destroyCurrentGame();
        startGame();
      };

      return;
    }
  }

  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void GameImpl::onMouseButtonUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void GameImpl::onMouseMove(const Vec2f& pos, const Vec2f&)
{
  int H = m_renderer.getViewportSize()[1];
  m_inputState.mouseX = (pos[0] - m_viewportOffset[0]) / H;
  m_inputState.mouseY = 1.f - (pos[1] - m_viewportOffset[1]) / H;
}

void GameImpl::onLeftStickMove(const Vec2f& delta)
{
  if (m_mobileControlsActive) {
    return;
  }

  const float threshold = 0.5;

  auto exceedsThreshold = [threshold](const Vec2f& d) {
    return fabs(d[0]) >= threshold || fabs(d[1]) >= threshold;
  };

  // Ignore multiple consecutive near zero magnitude deltas
  if (!exceedsThreshold(m_leftStickPrevDelta) && !exceedsThreshold(delta)) {
    m_leftStickPrevDelta = delta;
    return;
  }

  auto simulateKeypress = [this, threshold, delta](size_t dim, float neg, KeyboardKey key) {
    if (neg * delta[dim] >= threshold) {
      if (!m_inputState.keysPressed.contains(key)) {
        onKeyDown(key);
      }
    }
    else {
      if (m_inputState.keysPressed.contains(key)) {
        onKeyUp(key);
      }
    }
  };

  simulateKeypress(0, 1.f, KeyboardKey::Right);
  simulateKeypress(0, -1.f, KeyboardKey::Left);
  simulateKeypress(1, 1.f, KeyboardKey::Down);
  simulateKeypress(1, -1.f, KeyboardKey::Up);

  m_leftStickPrevDelta = delta;
}

void GameImpl::onRightStickMove(const Vec2f& delta)
{
  const float threshold = 0.1;
  const float sensitivity = 0.01;

  if (delta.squareMagnitude() >= threshold) {
    m_inputState.mouseX += delta[0] * sensitivity;
    m_inputState.mouseY += delta[1] * -sensitivity;
  }
}

void GameImpl::processKeyboardInput()
{
  static auto strPlayer = hashString("player");

  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs->system(BEHAVIOUR_SYSTEM));

  if (m_gameState == GameState::Playing) {
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
  }
}

void GameImpl::throwStick(float x, float y)
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

bool GameImpl::isInsideGameArea(float x, float) const
{
  return x >= 0.f && x < GAME_AREA_ASPECT;
}

void GameImpl::positionThrowingIndicator(const Vec2f& pos)
{
  if (!isInsideGameArea(pos[0], pos[1])) {
    return;
  }

  auto& t = m_ecs->componentStore().component<CLocalTransform>(m_scene.throwingModeIndicator);

  t.transform.set(0, 3, pos[0] - 0.025f);
  t.transform.set(1, 3, pos[1] - 0.025f);
}

void GameImpl::processMouseInput()
{
  if (m_gameState == GameState::Playing) {
    if (m_throwingMode) {
      float x = m_inputState.mouseX;
      float y = m_inputState.mouseY;

      positionThrowingIndicator({ x, y });

      if (!m_mobileControlsActive && m_inputState.mouseButtonsPressed.contains(MouseButton::Left)) {
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
  if (m_stateChangeFn.has_value()) {
    m_stateChangeFn.value()();
    m_stateChangeFn = std::nullopt;
  }

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
