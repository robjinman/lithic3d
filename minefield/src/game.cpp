#include "game_events.hpp"
#include "sys_grid.hpp"
#include "scene_builder.hpp"
#include "menu_system.hpp"
#include "game_options.hpp"
#include "b_player.hpp"
#include "mobile_controls.hpp"
#include "units.hpp"
#include "utils.hpp"
#include <fge/game.hpp>
#include <fge/engine.hpp>
#include <fge/file_system.hpp>
#include <fge/time.hpp>
#include <fge/logger.hpp>
#include <fge/audio_system.hpp>
#include <fge/renderer.hpp>
#include <fge/sys_animation_2d.hpp>
#include <fge/sys_behaviour.hpp>
#include <fge/sys_render_2d.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/sys_ui.hpp>
#include <fge/ecs.hpp>
#include <fge/systems.hpp>
#include <fge/events.hpp>
#include <fge/platform.hpp>
#ifdef DRM
#include <fge/drm.hpp>
#include <fge/product_activation.hpp>
#endif
#undef max
#undef min

using fge::EntityId;
using fge::NULL_ENTITY;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::Tick;
using fge::KeyboardKey;
using fge::GamepadButton;
using fge::MouseButton;
using fge::Vec2f;
using fge::Vec2i;
using fge::Rectf;
using fge::Recti;
using fge::SysRender2d;
using fge::SysSpatial;
using fge::SysAnimation2d;
using fge::SysBehaviour;

namespace
{

const auto strBang = hashString("bang");
const auto strCollect = hashString("collect");
const auto strEnterPortal = hashString("enter_portal");
const auto strScream = hashString("scream");
const auto strThrow = hashString("throw");
const auto strTick = hashString("tick");

const Tick deathPromptDelay = 0.66f * TICKS_PER_SECOND;

enum class GameState
{
  ProductActivation,
  Playing,
  Paused,
  MainMenu,
  Dead,
  DeadPaused
};

class GameImpl : public fge::Game
{
  public:
    GameImpl(fge::Engine& engine);

    float gameViewportAspectRatio() const override;
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
    fge::Engine& m_engine;
    GameOptionsManagerPtr m_options;
    MenuSystemPtr m_menuSystem;
    MobileControlsPtr m_mobileControls;
    bool m_mobileControlsActive = false;
#ifdef DRM
    fge::DrmPtr m_drm;
    fge::ProductActivationPtr m_productActivation;
#endif
    SceneBuilderPtr m_sceneBuilder;
    fge::InputState m_inputState;
    fge::Timer m_timer;
    Tick m_currentTick = 0;
    Tick m_timeSinceStart = 0;
    double m_measuredTickRate = 0;
    Scene m_scene;
    GameState m_gameState;
    bool m_shouldExit = false;
    bool m_throwingMode = false;
    Tick m_timeOfDeath = 0;
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
    void setScissor(uint32_t viewportW, uint32_t viewportH);
    void setMobileControlsScissor(uint32_t viewportW, uint32_t viewportH);
    bool isInsideGameArea(float x, float y) const;
    Vec2f throwingIndicatorPosition() const;
    Rectf calculateGameArea(uint32_t viewportW, uint32_t viewportH) const;
    bool showActivationScreenIfNotActivated();
};

GameImpl::GameImpl(fge::Engine& engine)
  : m_engine(engine)
{
  m_options = createGameOptionsManager(m_engine.fileSystem(), m_engine.logger());

  auto sysGrid = createSysGrid(m_engine.eventSystem());
  m_engine.ecs().addSystem(GRID_SYSTEM, std::move(sysGrid));

  m_menuSystem = createMenuSystem(m_engine.ecs(), m_engine.eventSystem(), *m_options,
    m_engine.logger(), fge::PLATFORM != fge::Platform::iOS);

  m_sceneBuilder = createSceneBuilder(m_engine.eventSystem(), m_engine.ecs(), *m_options);

  m_engine.setClearColour({ 0.01f, 0.01f, 0.02f, 1.f });

  auto viewport = m_engine.renderer().getViewportSize();

  m_engine.logger().info(STR("Initial viewport: " << viewport));

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
  m_mobileControls = createMobileControls(m_engine.ecs(), callbacks,
    calculateGameArea(viewport[0], viewport[1]));

  m_engine.eventSystem().listen([this](const Event& event) { handleEvent(event); });

  m_engine.renderer().start();

  setScissor(viewport[0], viewport[1]);
  setMobileControlsScissor(viewport[0], viewport[1]);

  m_engine.audioSystem().addMusic("sounds/music.ogg");
  m_engine.audioSystem().addSound(strBang, "sounds/bang.wav");
  m_engine.audioSystem().addSound(strCollect, "sounds/collect.wav");
  m_engine.audioSystem().addSound(strEnterPortal, "sounds/enter_portal.wav");
  m_engine.audioSystem().addSound(strScream, "sounds/scream.wav");
  m_engine.audioSystem().addSound(strThrow, "sounds/throw.wav");
  m_engine.audioSystem().addSound(strTick, "sounds/tick.wav");

  if (showActivationScreenIfNotActivated()) {
    m_engine.ecs().system<SysSpatial>().setEnabled(m_menuSystem->root(), true);

    m_menuSystem->showMainMenu();
    m_gameState = GameState::MainMenu;

    if (fge::MOBILE_PLATFORM) {
      showMobileControls();
    }

    m_engine.audioSystem().playMusic();
  }
}

bool GameImpl::showActivationScreenIfNotActivated()
{
#ifdef DRM
  m_drm = fge::createDrm("minefield", m_engine.fileSystem(), m_engine.logger());

  if (!m_drm->isActivated()) {
    m_productActivation = fge::createProductActivation(m_engine.ecs(), m_engine.eventSystem(),
      *m_drm, m_engine.logger());
    m_engine.ecs().system<SysSpatial>().setEnabled(m_productActivation->root(), true);
    m_gameState = GameState::ProductActivation;

    return false;
  }
#endif

  return true;
}

float GameImpl::gameViewportAspectRatio() const
{
  return 63.f / 48.f;
}

void GameImpl::hideMobileControls()
{
  if (m_mobileControlsActive) {
    m_engine.logger().info("Hiding mobile controls");
    m_mobileControlsActive = false;
    m_mobileControls->hide();
  }
}

void GameImpl::showMobileControls()
{
  if (!m_mobileControlsActive) {
    m_engine.logger().info("Showing mobile controls");
    m_mobileControlsActive = true;
    m_mobileControls->show();
  }
}

Rectf GameImpl::calculateGameArea(uint32_t viewportW, uint32_t viewportH) const
{
  float gameViewportRatio = gameViewportAspectRatio();

  float aspect = static_cast<float>(viewportW) / viewportH;
  float marginWithinViewport = (aspect - gameViewportRatio) / 2.f;

  return Rectf{
    .x = marginWithinViewport,
    .y = 0.f,
    .w = gameViewportRatio,
    .h = 1.f
  };
}

void GameImpl::setMobileControlsScissor(uint32_t viewportW, uint32_t viewportH)
{
  auto& sysRender = m_engine.ecs().system<SysRender2d>();

  Recti fullScreen{
    .x = 0,
    .y = 0,
    .w = static_cast<int>(viewportW),
    .h = static_cast<int>(viewportH)
  };

  sysRender.addScissor(MOBILE_CONTROLS_SCISSOR, fullScreen);
}

void GameImpl::setScissor(uint32_t viewportW, uint32_t viewportH)
{
  float gameAreaWidth = gameViewportAspectRatio() * viewportH;
  int x = static_cast<int>(0.5f * (static_cast<float>(viewportW) - gameAreaWidth));
  Recti scissor{
    .x = x,
    .y = 0,
    .w = static_cast<int>(gameAreaWidth),
    .h = static_cast<int>(viewportH)
  };
  m_engine.ecs().system<SysRender2d>()
    .addScissor(MAIN_SCISSOR, scissor);

  auto& margins = m_engine.renderer().getMargins();

  m_engine.logger().info(STR("Setting scissor " << MAIN_SCISSOR << ": " << scissor));

  m_viewportOffset = {
    scissor.x + static_cast<int>(margins.left),
    scissor.y + static_cast<int>(margins.top)
  };

  m_engine.logger().info(STR("Viewport offset: " << m_viewportOffset));
}

void GameImpl::onWindowResize(uint32_t w, uint32_t h)
{
  auto& margins = m_engine.renderer().getMargins();

  float viewportW = w - margins.left - margins.right;
  float viewportH = h - margins.top - margins.bottom;

  auto gameArea = calculateGameArea(viewportW, viewportH);

  m_engine.logger().info(STR("Window resized (w: " << w << ", h: " << h << ")"));
  m_engine.logger().info(STR("Screen margins (l: " << margins.left << ", r: " << margins.right
    << ", t: " << margins.top << ", b: " << margins.bottom << ")"));
  m_engine.logger().info(STR("Viewport (w: " << viewportW << ", h: " << viewportH << ")"));
  m_engine.logger().info(STR("Game area (x: " << gameArea.x << ", y: " << gameArea.y << ", w: "
    << gameArea.w << ", h: " << gameArea.h << ")"));

  setScissor(viewportW, viewportH);
  setMobileControlsScissor(viewportW, viewportH);

  m_mobileControls->setGameArea(gameArea);

  m_engine.ecs().system<SysRender2d>().onResize();
}

void GameImpl::handleMenuEvent(const Event& event)
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

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
  m_engine.ecs().componentStore()
    .component<fge::CRender2d>(m_scene.throwingModeIndicator).visible = on;

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

  m_engine.ecs().processEvent(event);

  if (event.name == g_strPlayerDeath) {
    onPlayerDeath();
  }
#ifdef DRM
  else if (event.name == fge::g_strProductActivate) {
    auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
    
    sysSpatial.setEnabled(m_productActivation->root(), false);
    sysSpatial.setEnabled(m_menuSystem->root(), true);
    m_menuSystem->showMainMenu();
    m_gameState = GameState::MainMenu;

    m_engine.audioSystem().playMusic();
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
    m_engine.ecs().removeEntity(id);
  }
  m_engine.ecs().removeEntity(m_scene.worldRoot);

  m_engine.eventSystem().dropEvents();

  m_engine.audioSystem().stopAllSounds();
}

void GameImpl::startGame()
{
  m_scene = m_sceneBuilder->buildScene(m_menuSystem->difficultyLevel());
  m_gameState = GameState::Playing;
  m_timeSinceStart = 0;
}

void GameImpl::onPlayerDeath()
{
  m_gameState = GameState::Dead;
  m_timeOfDeath = m_currentTick;

  toggleThrowingMode(false);
}

void GameImpl::onPlayerVictorious()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

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
    m_engine.audioSystem().playSound(strBang);
  }
  else if (event.name == g_strEnterPortal) {
    m_engine.audioSystem().playSound(strEnterPortal);
  }
  else if (event.name == g_strTenSecondsRemaining) {
    m_engine.audioSystem().playSound(strTick);
  }
  else if (event.name == g_strThrow) {
    m_engine.audioSystem().playSound(strThrow);
  }
  else if (event.name == g_strTimeout || event.name == g_strWandererAttack) {
    m_engine.audioSystem().playSound(strScream);
  }
  else if (event.name == g_strItemCollect) {
    auto& e = dynamic_cast<const EItemCollect&>(event);
    if (e.value > 0) {
      m_engine.audioSystem().playSound(strCollect);
    }
  }
}

Vec2f GameImpl::throwingIndicatorPosition() const
{
  const auto& t = m_engine.ecs().componentStore()
    .component<fge::CLocalTransform>(m_scene.throwingModeIndicator);

  return {
    t.transform.at(0, 3) + 0.025f,
    t.transform.at(1, 3) + 0.025f
  };
}

void GameImpl::onKeyDown(KeyboardKey key)
{
  switch (key) {
    case KeyboardKey::F: {
      m_engine.logger().info(STR("Simulation tick rate: " << m_measuredTickRate));
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
      auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

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

  if (m_gameState == GameState::Dead && m_currentTick - m_timeOfDeath >= deathPromptDelay) {
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

  if (m_gameState == GameState::Dead && button != GamepadButton::B
    && m_currentTick - m_timeOfDeath >= deathPromptDelay) {

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
    if (isInsideGameArea(m_inputState.mouseX, m_inputState.mouseY)
      && m_currentTick - m_timeOfDeath >= deathPromptDelay) {

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
  int H = m_engine.renderer().getViewportSize()[1];
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

  auto& sysBehaviour = m_engine.ecs().system<SysBehaviour>();

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
  auto& sysAnimation = m_engine.ecs().system<SysAnimation2d>();

  if (sysAnimation.hasAnimationPlaying(m_stickId)) {
    return;
  }

  auto& sysGrid = m_engine.ecs().system<SysGrid>();

  Vec2i dest{ static_cast<int>(x / GRID_CELL_W), static_cast<int>(y / GRID_CELL_H) };

  if (sysGrid.isInRange(dest[0], dest[1])) {
    m_engine.eventSystem().raiseEvent(EThrow{m_stickId, dest[0], dest[1]});
    toggleThrowingMode(false);
  }
}

bool GameImpl::isInsideGameArea(float x, float) const
{
  return x >= 0.f && x < gameViewportAspectRatio();
}

void GameImpl::positionThrowingIndicator(const Vec2f& pos)
{
  if (!isInsideGameArea(pos[0], pos[1])) {
    return;
  }

  auto& t = m_engine.ecs().componentStore()
    .component<fge::CLocalTransform>(m_scene.throwingModeIndicator);

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
        m_engine.eventSystem().raiseEvent(ETimerTick{timeRemaining});
      }

      if (secondsElapsed >= gameDuration) {
        m_engine.eventSystem().raiseEvent(ETimeout{});
      }
    }
  }
}

void GameImpl::adjustVolume()
{
  if (m_menuSystem->musicVolume() != m_musicVolume) {
    m_musicVolume = m_menuSystem->musicVolume();
    m_engine.audioSystem().setMusicVolume(m_musicVolume);
  }

  if (m_menuSystem->sfxVolume() != m_sfxVolume) {
    m_sfxVolume = m_menuSystem->sfxVolume();
    m_engine.audioSystem().setSoundVolume(m_sfxVolume);
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

  if (m_gameState == GameState::Dead || m_gameState == GameState::DeadPaused) {
    if (m_currentTick - m_timeOfDeath >= deathPromptDelay) {
      m_engine.ecs().system<SysSpatial>()
        .setEnabled(m_scene.restartGamePrompt, true);
    }
  }

  m_engine.update(m_currentTick, m_inputState);
  m_menuSystem->update();

  adjustVolume();

  return !m_shouldExit;
}

} // namespace

fge::GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Minefield",
    .appShortName = "minefield",
    .vendorShortName = "freeholdapps",
    .windowW = 630,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080
  };
}

fge::GamePtr createGame(fge::Engine& engine)
{
  return std::make_unique<GameImpl>(engine);
}
