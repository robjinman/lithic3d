#include <lithic3d/engine.hpp>
#include <lithic3d/logger.hpp>
#include <lithic3d/game.hpp>
#include <lithic3d/renderer.hpp>
#include <lithic3d/time.hpp>
#include <lithic3d/window_delegate.hpp>
#include <lithic3d/file_system.hpp>
#include <lithic3d/platform_paths.hpp>
#include <lithic3d/audio_system.hpp>
#include <lithic3d/units.hpp>
#include <lithic3d/shader_manifest.hpp>
#include <GLFW/glfw3.h>
#if defined(_WIN32) && defined(NDEBUG)
#include <windows.h>
#endif
#include <iostream>

namespace lithic3d
{

WindowDelegatePtr createWindowDelegate(GLFWwindow& window);
PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

namespace
{

struct WindowState
{
  int posX = 0;
  int posY = 0;
  int width = 0;
  int height = 0;
};

enum class ControlMode
{
  KeyboardMouse,
  Gamepad
};

GamepadButton buttonCode(int button)
{
  switch (button) {
    case GLFW_GAMEPAD_BUTTON_A:             return GamepadButton::A;
    case GLFW_GAMEPAD_BUTTON_B:             return GamepadButton::B;
    case GLFW_GAMEPAD_BUTTON_X:             return GamepadButton::X;
    case GLFW_GAMEPAD_BUTTON_Y:             return GamepadButton::Y;
    case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:   return GamepadButton::L1;
    case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER:  return GamepadButton::R1;
    case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:     return GamepadButton::Left;
    case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:    return GamepadButton::Right;
    case GLFW_GAMEPAD_BUTTON_DPAD_UP:       return GamepadButton::Up;
    case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:     return GamepadButton::Down;
    // ...
    default:                                return GamepadButton::Unknown;
  }
}

class Application
{
  public:
    Application();

    void run();
    void onKeyboardInput(int key, int action);
    void onMouseMove(float x, float y);
    void onMouseClick();
    void onJoystickEvent(int event);
    void onWindowResize(int w, int h);

    ~Application();

  private:
    static Application* m_instance;
    static void onKeyboardInput(GLFWwindow* window, int code, int, int action, int);
    static void onMouseMove(GLFWwindow*, double x, double y);
    static void onMouseClick(GLFWwindow* window, int, int, int);
    static void onJoystickEvent(int, int event);
    static void onWindowResize(GLFWwindow* window, int w, int h);

    GLFWwindow* m_window;
    GameConfig m_config;
    EnginePtr m_engine;
    GamePtr m_game;

    bool m_fullscreen = false;
    bool m_inputCaptured = false;
    WindowState m_initialWindowState;
    ControlMode m_controlMode;
    Vec2f m_lastMousePos;
    GLFWgamepadstate m_gamepadState;

    void enterInputCapture();
    void exitInputCapture();
    void toggleFullScreen();
    Vec2i windowSize() const;
    void processGamepadInput();
    void cleanUp();
};

Application* Application::m_instance = nullptr;

void Application::onKeyboardInput(GLFWwindow*, int key, int, int action, int)
{
  if (m_instance) {
    m_instance->onKeyboardInput(key, action);
  }
}

void Application::onMouseMove(GLFWwindow*, double x, double y)
{
  if (m_instance) {
    m_instance->onMouseMove(static_cast<float>(x), static_cast<float>(y));
  }
}

void Application::onMouseClick(GLFWwindow*, int, int, int)
{
  if (m_instance) {
    m_instance->onMouseClick();
  }
}

void Application::onJoystickEvent(int, int event)
{
  if (m_instance) {
    m_instance->onJoystickEvent(event);
  }
}

void Application::onWindowResize(GLFWwindow*, int w, int h)
{
  if (m_instance) {
    m_instance->onWindowResize(w, h);
  }
}

Application::Application()
{
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Don't create OpenGL context
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  m_instance = this;

  m_config = getGameConfig();

  m_window = glfwCreateWindow(m_config.windowW, m_config.windowH, m_config.appDisplayName.c_str(),
    nullptr, nullptr);
  glfwGetWindowPos(m_window, &m_initialWindowState.posX, &m_initialWindowState.posY);
  glfwGetWindowSize(m_window, &m_initialWindowState.width, &m_initialWindowState.height);

  glfwSetWindowSizeCallback(m_window, onWindowResize);

  m_controlMode = glfwJoystickPresent(GLFW_JOYSTICK_1) ?
    ControlMode::Gamepad :
    ControlMode::KeyboardMouse;

  auto platformPaths = createPlatformPaths(m_config.appShortName, m_config.vendorShortName);
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));
  auto windowDelegate = createWindowDelegate(*m_window);
  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto audioSystem = createAudioSystem(*fileSystem, *logger);
  auto resourceManager = createResourceManager(*logger);
  auto renderer = createRenderer(std::move(windowDelegate), *resourceManager, *fileSystem, *logger,
    {});

  logger->info("Compiling shaders...");

  auto manifest = fileSystem->readAppDataFile(m_config.shaderManifest);
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = createEngine(m_config.drawDistance, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));

  glfwSetMouseButtonCallback(m_window, onMouseClick);

  glfwSetKeyCallback(m_window, Application::onKeyboardInput);
  glfwSetCursorPosCallback(m_window, Application::onMouseMove);
  glfwSetJoystickCallback(Application::onJoystickEvent);

  try {
    m_game = createGame(*m_engine);
  }
  catch (...) {
    cleanUp();
    throw;
  }
}

void Application::cleanUp()
{
  m_engine->resourceManager().deactivate();

  m_game.reset();
  m_engine.reset();
  glfwDestroyWindow(m_window);
  glfwTerminate();
}

void Application::run()
{
  FrameRateLimiter frameRateLimiter{TICKS_PER_SECOND};

  while(!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    if (!m_game->update()) {
      break;
    }

    if (m_controlMode == ControlMode::Gamepad) {
      processGamepadInput();
    }

    frameRateLimiter.wait();
  }
}

Vec2i Application::windowSize() const
{
  Vec2i size;
  glfwGetWindowSize(m_window, &size[0], &size[1]);
  return size;
}

void Application::onJoystickEvent(int event)
{
  m_engine->logger().info(STR("Received joystick event: " << event));

  switch (event) {
    case GLFW_CONNECTED:
      exitInputCapture();
      m_controlMode = ControlMode::Gamepad;
      enterInputCapture();
      break;
    case GLFW_DISCONNECTED:
      exitInputCapture();
      m_controlMode = ControlMode::KeyboardMouse;
      enterInputCapture();
      break;
  }
}

void Application::onKeyboardInput(int code, int action)
{
  auto key = static_cast<KeyboardKey>(code);

  if (action == GLFW_PRESS) {
    m_game->onKeyDown(key);

    switch (key) {
      case KeyboardKey::Escape:
        exitInputCapture();
        break;
      case KeyboardKey::F:
        m_engine->logger().info(STR("Renderer frame rate: " << m_engine->renderer().frameRate()));
        break;
#ifdef __APPLE__
      case KeyboardKey::F12:
#else
      case KeyboardKey::F11:
#endif
        toggleFullScreen();
        break;
      default: break;
    }
  }
  else if (action == GLFW_RELEASE) {
    m_game->onKeyUp(key);
  }
}

void Application::toggleFullScreen()
{
  if (m_fullscreen) {
    m_engine->logger().info("Toggling fullscreen mode OFF");

    glfwSetWindowMonitor(m_window, NULL, m_initialWindowState.posX, m_initialWindowState.posY,
      m_initialWindowState.width, m_initialWindowState.height, 0);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    m_engine->onWindowResize(m_initialWindowState.width, m_initialWindowState.height);
    m_game->onWindowResize(m_initialWindowState.width, m_initialWindowState.height);

    m_fullscreen = false;
  }
  else {
    m_engine->logger().info("Toggling fullscreen mode ON");

    glfwGetWindowPos(m_window, &m_initialWindowState.posX, &m_initialWindowState.posY);
    glfwGetWindowSize(m_window, &m_initialWindowState.width, &m_initialWindowState.height);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwSetWindowMonitor(m_window, monitor, 0, 0, m_config.fullscreenResolutionW,
      m_config.fullscreenResolutionH, mode->refreshRate);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    m_engine->onWindowResize(m_config.fullscreenResolutionW, m_config.fullscreenResolutionH);
    m_game->onWindowResize(m_config.fullscreenResolutionW, m_config.fullscreenResolutionH);

    m_fullscreen = true;
  }
}

void Application::onWindowResize(int w, int h)
{
  m_engine->onWindowResize(w, h);
  m_game->onWindowResize(w, h);
}

void Application::onMouseMove(float x, float y)
{
  if (m_config.captureMouse && !m_inputCaptured) {
    return;
  }

  Vec2f pos{ x, y };
  Vec2f delta = (pos - m_lastMousePos) / static_cast<Vec2f>(windowSize());
  m_game->onMouseMove(pos, delta);
  m_lastMousePos = { x, y };
}

void Application::onMouseClick()
{
  if (m_config.captureMouse && !m_inputCaptured) {
    enterInputCapture();
    return;
  }

  auto buttonState = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_1);
  if (buttonState == GLFW_PRESS) {
    m_game->onMouseButtonDown();
  }
  else {
    m_game->onMouseButtonUp();
  }
}

void Application::enterInputCapture()
{
  if (!m_config.captureMouse) {
    return;
  }

  glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(m_window, Application::onKeyboardInput);
  glfwSetCursorPosCallback(m_window, Application::onMouseMove);
  glfwSetJoystickCallback(Application::onJoystickEvent);

  double x = 0;
  double y = 0;
  glfwGetCursorPos(m_window, &x, &y);
  m_lastMousePos = { static_cast<float>(x), static_cast<float>(y) };

  m_inputCaptured = true;
}

void Application::processGamepadInput()
{
  GLFWgamepadstate state;

  if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
    for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
      if (m_gamepadState.buttons[i] == GLFW_RELEASE && state.buttons[i] == GLFW_PRESS) {
        m_game->onButtonDown(buttonCode(i));
      }
      else if (m_gamepadState.buttons[i] == GLFW_PRESS && state.buttons[i] == GLFW_RELEASE) {
        m_game->onButtonUp(buttonCode(i));
      }
    }
  }

  float leftX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
  float leftY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
  m_game->onLeftStickMove({ leftX, leftY });

  float rightX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
  float rightY = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
  m_game->onRightStickMove({ rightX, rightY });

  m_gamepadState = state;
}

void Application::exitInputCapture()
{
  if (!m_config.captureMouse) {
    return;
  }

  glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(m_window, nullptr);
  glfwSetCursorPosCallback(m_window, nullptr);

  m_inputCaptured = false;
}

Application::~Application()
{
  cleanUp();
}

} // namespace
} // namespace lithic3d

#if defined(_WIN32) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int)
#else
int main()
#endif
{
  try {
    lithic3d::Application app;
    app.run();
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
