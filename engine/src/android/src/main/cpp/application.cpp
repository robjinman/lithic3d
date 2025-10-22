#include "logger.hpp"
#include "game.hpp"
#include "renderer.hpp"
#include "time.hpp"
#include "utils.hpp"
#include "units.hpp"
#include "window_delegate.hpp"
#include "file_system.hpp"
#include "audio_system.hpp"
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <android/log.h>
#include <iostream>

LoggerPtr createAndroidLogger();
WindowDelegatePtr createWindowDelegate(ANativeWindow& window);
FileSystemPtr createAndroidFileSystem(const std::filesystem::path& userDataPath,
  AAssetManager& assetManager);

namespace
{

// From https://stackoverflow.com/a/50831255
void autoHideNavBar(android_app* state)
{
  JNIEnv* env{};
  state->activity->vm->AttachCurrentThread(&env, NULL);

  jclass activityClass = env->FindClass("android/app/NativeActivity");
  jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");

  jclass windowClass = env->FindClass("android/view/Window");
  jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");

  jclass viewClass = env->FindClass("android/view/View");
  jmethodID setSystemUiVisibility = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");

  jobject window = env->CallObjectMethod(state->activity->clazz, getWindow);

  jobject decorView = env->CallObjectMethod(window, getDecorView);

  jfieldID flagFullscreenID = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I");
  jfieldID flagHideNavigationID = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION",
    "I");
  jfieldID flagImmersiveStickyID = env->GetStaticFieldID(viewClass,
    "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I");

  const int flagFullscreen = env->GetStaticIntField(viewClass, flagFullscreenID);
  const int flagHideNavigation = env->GetStaticIntField(viewClass, flagHideNavigationID);
  const int flagImmersiveSticky = env->GetStaticIntField(viewClass, flagImmersiveStickyID);
  const int flag = flagFullscreen | flagHideNavigation | flagImmersiveSticky;

  env->CallVoidMethod(decorView, setSystemUiVisibility, flag);

  state->activity->vm->DetachCurrentThread();
}

GamepadButton buttonCode(int32_t key)
{
  switch (key) {
    case AKEYCODE_BUTTON_A:   return GamepadButton::A;
    case AKEYCODE_BUTTON_B:   return GamepadButton::B;
    case AKEYCODE_BUTTON_X:   return GamepadButton::X;
    case AKEYCODE_BUTTON_Y:   return GamepadButton::Y;
    case AKEYCODE_BUTTON_L1:  return GamepadButton::L1;
    case AKEYCODE_BUTTON_L2:  return GamepadButton::L2;
    case AKEYCODE_BUTTON_R1:  return GamepadButton::R1;
    case AKEYCODE_BUTTON_R2:  return GamepadButton::R2;
    case AKEYCODE_DPAD_UP:    return GamepadButton::Up;
    case AKEYCODE_DPAD_DOWN:  return GamepadButton::Down;
    case AKEYCODE_DPAD_LEFT:  return GamepadButton::Left;
    case AKEYCODE_DPAD_RIGHT: return GamepadButton::Right;
    default: return GamepadButton::Unknown;
  }
}

KeyboardKey keyCode(int32_t key)
{
  if (key >= AKEYCODE_A && key <= AKEYCODE_Z) {
    auto i = static_cast<std::underlying_type_t<KeyboardKey>>(KeyboardKey::A) + (key - AKEYCODE_A);
    return static_cast<KeyboardKey>(i);
  }

  switch (key) {
    case AKEYCODE_SPACE: return KeyboardKey::Space;
    // ...
    default: return KeyboardKey::Unknown;
  }
}

class Application
{
  public:
    Application(WindowDelegatePtr windowDelegate, FileSystemPtr fileSystem, Logger& logger);

    bool update();
    void onConfigChange();
    void onLeftStickMove(float x, float y);
    void onRightStickMove(float x, float y);
    void onKeyDown(KeyboardKey key);
    void onKeyUp(KeyboardKey key);
    void onButtonDown(GamepadButton button);
    void onButtonUp(GamepadButton button);
    void onMouseMove(float x, float y);
    void onMouseButtonDown(float x, float y);
    void onMouseButtonUp();
    void hideMobileControls();

  private:
    Logger& m_logger;
    WindowDelegatePtr m_windowDelegate;
    FileSystemPtr m_fileSystem;
    AudioSystemPtr m_audioSystem;
    render::RendererPtr m_renderer;
    GamePtr m_game;
    Vec2f m_leftStickDelta;
    Vec2f m_rightStickDelta;
    Vec2i m_screenSize;
};

using ApplicationPtr = std::unique_ptr<Application>;

Application::Application(WindowDelegatePtr windowDelegate, FileSystemPtr fileSystem, Logger& logger)
  : m_logger(logger)
  , m_windowDelegate(std::move(windowDelegate))
  , m_fileSystem(std::move(fileSystem))
{
  m_audioSystem = createAudioSystem(*m_fileSystem);
  m_renderer = createRenderer(*m_fileSystem, *m_windowDelegate, m_logger, {});
  m_screenSize = m_renderer->getScreenSize();

  m_game = createGame(*m_renderer, *m_audioSystem, *m_fileSystem, m_logger);
}

bool Application::update()
{
  auto screenSize = m_renderer->getScreenSize();
  if (screenSize != m_screenSize) {
    m_game->onWindowResize(screenSize[0], screenSize[1]);
    m_screenSize = screenSize;
  }

  m_game->onLeftStickMove(m_leftStickDelta);
  m_game->onRightStickMove(m_rightStickDelta);
  return m_game->update();
}

void Application::hideMobileControls()
{
  m_game->hideMobileControls();
}

void Application::onConfigChange()
{
  m_renderer->onResize();
}

void Application::onLeftStickMove(float x, float y)
{
  m_leftStickDelta = { x, y };
}

void Application::onRightStickMove(float x, float y)
{
  m_rightStickDelta = { x, y };
}

void Application::onKeyDown(KeyboardKey key)
{
  m_game->onKeyDown(key);
}

void Application::onKeyUp(KeyboardKey key)
{
  m_game->onKeyUp(key);
}

void Application::onButtonDown(GamepadButton button)
{
  m_game->onButtonDown(button);

  if (button == GamepadButton::Y) {
    m_logger.info(STR("Renderer frame rate: " << m_renderer->frameRate()));
  }
}

void Application::onButtonUp(GamepadButton button)
{
  m_game->onButtonUp(button);
}

void Application::onMouseMove(float x, float y)
{
  m_game->onMouseMove({ x, y }, {});
}

void Application::onMouseButtonDown(float x, float)
{
  auto aspect = m_game->gameViewportAspectRatio();

  auto viewport = m_renderer->getViewportSize();
  float screenAspect = static_cast<float>(viewport[0]) / viewport[1];

  float xNorm = x / viewport[1];
  float x0 = (screenAspect - aspect) / 2.f;
  float x1 = x0 + aspect;

  if (xNorm < x0 || xNorm > x1) {
    m_game->showMobileControls();
  }

  m_game->onMouseButtonDown();
}

void Application::onMouseButtonUp()
{
  m_game->onMouseButtonUp();
}

ApplicationPtr createApplication(android_app& state, Logger& logger)
{
  ASSERT(state.window != nullptr, "Window is NULL");

  auto windowDelegate = createWindowDelegate(*state.window);
  FileSystemPtr fileSystem = createAndroidFileSystem(state.activity->internalDataPath,
    *state.activity->assetManager);
  return std::make_unique<Application>(std::move(windowDelegate), std::move(fileSystem), logger);
}

class EventHandler
{
  public:
    EventHandler(Logger& logger);

    void setApplication(Application* app);

    void onCommandEvent(int32_t command);
    int32_t onInputEvent(const AInputEvent& event);

    bool isWindowInitialised() const;

  private:
    Logger& m_logger;
    Application* m_app = nullptr;
    bool m_windowInitialised = false;
};

EventHandler::EventHandler(Logger& logger)
  : m_logger(logger)
{
}

void EventHandler::setApplication(Application* app)
{
  m_app = app;
}

void EventHandler::onCommandEvent(int32_t command)
{
  if (command == APP_CMD_INIT_WINDOW) {
    m_windowInitialised = true;
    return;
  }

  if (m_app) {
    switch (command) {
      case APP_CMD_CONFIG_CHANGED:
        m_app->onConfigChange();
        break;
      default: break;
    }
  }
}

int32_t EventHandler::onInputEvent(const AInputEvent& event)
{
  if (m_app) {
    int32_t eventType = AInputEvent_getType(&event);

    if (eventType == AINPUT_EVENT_TYPE_KEY) {
      int32_t action = AKeyEvent_getAction(&event);
      int32_t key = AKeyEvent_getKeyCode(&event);

      if (action == AKEY_EVENT_ACTION_DOWN) {
        auto button = buttonCode(key);
        if (button != GamepadButton::Unknown) {
          m_app->onButtonDown(button);
        }
        else {
          m_app->onKeyDown(keyCode(key));
        }
      }
      else if (action == AKEY_EVENT_ACTION_UP) {
        auto button = buttonCode(key);
        if (button != GamepadButton::Unknown) {
          m_app->onButtonUp(button);
        }
        else {
          m_app->onKeyUp(keyCode(key));
        }
      }

      return 1;
    }
    if (eventType == AINPUT_EVENT_TYPE_MOTION) {
      auto src = AInputEvent_getSource(&event);

      if (src == AINPUT_SOURCE_JOYSTICK || src == AINPUT_SOURCE_GAMEPAD
        || src == AINPUT_SOURCE_DPAD) {

        m_app->hideMobileControls();

        float threshold = 0.1f;

        float hatX = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_HAT_X, 0);
        float hatY = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_HAT_Y, 0);

        if (fabs(hatX) >= threshold || fabs(hatY) >= threshold) {
          m_app->onLeftStickMove(hatX, hatY);
        }
        else {
          float x = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_X, 0);
          float y = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_Y, 0);

          m_app->onLeftStickMove(x, y);
        }

        float z = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_Z, 0);
        float rz = AMotionEvent_getAxisValue(&event, AMOTION_EVENT_AXIS_RZ, 0);

        m_app->onRightStickMove(z, rz);
      }
      else if (src & AINPUT_SOURCE_CLASS_POINTER) {
        float x = AMotionEvent_getX(&event, 0);
        float y = AMotionEvent_getY(&event, 0);

        int32_t action = AMotionEvent_getAction(&event) & AMOTION_EVENT_ACTION_MASK;
        switch (action) {
          case AMOTION_EVENT_ACTION_DOWN:
            m_app->onMouseMove(x, y);
            m_app->onMouseButtonDown(x, y);
            break;
          case AMOTION_EVENT_ACTION_UP:
            m_app->onMouseMove(x, y);
            m_app->onMouseButtonUp();
            break;
          case AMOTION_EVENT_ACTION_MOVE: {
            m_app->onMouseMove(x, y);
            break;
          }
        }
      }

      return 1;
    }
  }

  return 0;
}

bool EventHandler::isWindowInitialised() const
{
  return m_windowInitialised;
}

bool waitForWindow(android_app& state, EventHandler& handler)
{
  FrameRateLimiter frameRateLimiter{TICKS_PER_SECOND};

  while (!state.destroyRequested) {
    android_poll_source* source = nullptr;
    ALooper_pollOnce(0, nullptr, nullptr, reinterpret_cast<void**>(&source));

    if (source != nullptr) {
      source->process(&state, source);
    }

    if (handler.isWindowInitialised()) {
      return true;
    }

    frameRateLimiter.wait();
  }

  return false;
}

void handleCommand(android_app* state, int32_t command)
{
  reinterpret_cast<EventHandler*>(state->userData)->onCommandEvent(command);
}

int32_t handleInput(android_app* state, AInputEvent* event)
{
  return reinterpret_cast<EventHandler*>(state->userData)->onInputEvent(*event);
}

} // namespace

void android_main(android_app* state)
{
  autoHideNavBar(state);

  auto logger = createAndroidLogger();
  logger->info("Starting Minefield");

  EventHandler handler{*logger};

  state->onAppCmd = handleCommand;
  state->onInputEvent = handleInput;
  state->userData = &handler;

  if (!waitForWindow(*state, handler)) {
    return;
  }

  auto application = createApplication(*state, *logger);
  handler.setApplication(application.get());

  FrameRateLimiter frameRateLimiter{TICKS_PER_SECOND};

  while (!state->destroyRequested) {
    android_poll_source* source = nullptr;
    ALooper_pollOnce(0, nullptr, nullptr, reinterpret_cast<void**>(&source));

    if (source != nullptr) {
      source->process(state, source);
    }

    if (!application->update()) {
      ANativeActivity_finish(state->activity);
    }

    frameRateLimiter.wait();
  }
}
