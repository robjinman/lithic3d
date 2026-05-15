#include <lithic3d/lithic3d.hpp>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <fcntl.h>
#include <iostream>

namespace lithic3d
{

WindowDelegatePtr createWindowDelegate();
PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

namespace
{

const int SCREEN_W = 1920;
const int SCREEN_H = 1080;

GamepadButton eventCodeToButton(int code)
{
  switch (code) {
    case BTN_NORTH: return GamepadButton::X;
    case BTN_EAST: return GamepadButton::B;
    case BTN_SOUTH: return GamepadButton::A;
    case BTN_WEST: return GamepadButton::Y;
    // TODO
    // ...
    default: return GamepadButton::Unknown;
  }
}

KeyboardKey eventCodeToKey(int code)
{
  switch (code) {
    case KEY_NUMERIC_0: return KeyboardKey::Num0;
    case KEY_NUMERIC_1: return KeyboardKey::Num1;
    case KEY_NUMERIC_2: return KeyboardKey::Num2;
    case KEY_NUMERIC_3: return KeyboardKey::Num3;
    case KEY_NUMERIC_4: return KeyboardKey::Num4;
    case KEY_NUMERIC_5: return KeyboardKey::Num5;
    case KEY_NUMERIC_6: return KeyboardKey::Num6;
    case KEY_NUMERIC_7: return KeyboardKey::Num7;
    case KEY_NUMERIC_8: return KeyboardKey::Num8;
    case KEY_NUMERIC_9: return KeyboardKey::Num9;
    case KEY_A: return KeyboardKey::A;
    case KEY_B: return KeyboardKey::B;
    case KEY_C: return KeyboardKey::C;
    case KEY_D: return KeyboardKey::D;
    case KEY_E: return KeyboardKey::E;
    case KEY_F: return KeyboardKey::F;
    case KEY_G: return KeyboardKey::G;
    case KEY_H: return KeyboardKey::H;
    case KEY_I: return KeyboardKey::I;
    case KEY_J: return KeyboardKey::J;
    case KEY_K: return KeyboardKey::K;
    case KEY_L: return KeyboardKey::L;
    case KEY_M: return KeyboardKey::M;
    case KEY_N: return KeyboardKey::N;
    case KEY_O: return KeyboardKey::O;
    case KEY_P: return KeyboardKey::P;
    case KEY_Q: return KeyboardKey::Q;
    case KEY_R: return KeyboardKey::R;
    case KEY_S: return KeyboardKey::S;
    case KEY_T: return KeyboardKey::T;
    case KEY_U: return KeyboardKey::U;
    case KEY_V: return KeyboardKey::V;
    case KEY_W: return KeyboardKey::W;
    case KEY_X: return KeyboardKey::X;
    case KEY_Y: return KeyboardKey::Y;
    case KEY_Z: return KeyboardKey::Z;
    case KEY_SPACE: return KeyboardKey::Space;
    case KEY_ESC: return KeyboardKey::Escape;
    case KEY_ENTER: return KeyboardKey::Enter;
    case KEY_BACKSPACE: return KeyboardKey::Backspace;
    case KEY_F1: return KeyboardKey::F1;
    case KEY_F2: return KeyboardKey::F2;
    case KEY_F3: return KeyboardKey::F3;
    case KEY_F4: return KeyboardKey::F4;
    case KEY_F5: return KeyboardKey::F5;
    case KEY_F6: return KeyboardKey::F6;
    case KEY_F7: return KeyboardKey::F7;
    case KEY_F8: return KeyboardKey::F8;
    case KEY_F9: return KeyboardKey::F9;
    case KEY_F10: return KeyboardKey::F10;
    case KEY_F11: return KeyboardKey::F11;
    case KEY_F12: return KeyboardKey::F12;
    case KEY_RIGHT: return KeyboardKey::Right;
    case KEY_LEFT: return KeyboardKey::Left;
    case KEY_DOWN: return KeyboardKey::Down;
    case KEY_UP: return KeyboardKey::Up;
    default: return KeyboardKey::Unknown;
  }
}

struct InputDevice
{
  int fileDescriptor = 0;
  libevdev* device = nullptr;
};

class Application
{
  public:
    Application();

    void run();

    ~Application();

  private:
    GameConfig m_config;
    WindowDelegatePtr m_windowDelegate;
    EnginePtr m_engine;
    GamePtr m_game;
    std::vector<InputDevice> m_devices;
    Vec2f m_prevMousePos = { 0.5f * SCREEN_W, 0.5f * SCREEN_H };
    Vec2f m_mouseDelta;
    Vec2f m_currentLeftStickPos;
    Vec2f m_currentRightStickPos;

    void enumerateInputDevices();
    void pollEvents();
    void handleEvent(const input_event& event);
    void applyInputDeltas();
    void cleanUp();
};

Application::Application()
{
  m_config = getGameConfig();

  auto platformPaths = createPlatformPaths(m_config.appShortName, m_config.vendorShortName);
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));
  fillDefaultPaths(*fileSystem, m_config.paths);
  m_windowDelegate = createWindowDelegate();
  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto audioSystem = createAudioSystem(m_config.paths.soundsDir, *logger);
  auto resourceManager = createResourceManager(*logger);
  auto renderer = createRenderer(*m_windowDelegate, *resourceManager, m_config.paths, *logger, {});

  logger->info("Compiling shaders...");

  auto manifest = m_config.paths.shaderManifest.read();
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = createEngine(m_config, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));

  try {
    m_game = createGame(*m_engine);
  }
  catch (...) {
    cleanUp();
    throw;
  }

  enumerateInputDevices();
}

void Application::enumerateInputDevices()
{
  const std::string base = "/dev/input/event";
  for (auto const& entry : std::filesystem::directory_iterator{"/dev/input"}) {
    auto path = entry.path().string();
    if (path.starts_with(base)) {
      auto i = std::stoul(path.substr(base.length()));
      if (i + 1 > m_devices.size()) {
        m_devices.resize(i + 1);
      }

      m_devices[i].fileDescriptor = open(path.c_str(), O_RDONLY | O_NONBLOCK);

      if (libevdev_new_from_fd(m_devices[i].fileDescriptor, &m_devices[i].device) < 0) {
        m_devices[i].device = nullptr;
        m_engine->logger().warn("Error opening input device. Ignoring");
        continue;
      }

      m_engine->logger().info(STR("Input device: " << libevdev_get_name(m_devices[i].device)));
    }
  }
}

void Application::handleEvent(const input_event& event)
{
  switch (event.type) {
    case EV_KEY: {
      if (event.value == 1) {
        auto key = eventCodeToKey(event.code);
        if (key != KeyboardKey::Unknown) {
          m_game->onKeyDown(key);
        }
        else {
          auto btn = eventCodeToButton(event.code);
          if (btn != GamepadButton::Unknown) {
            m_game->onButtonDown(btn);
          }
        }
      }
      else if (event.value == 0) {
        auto key = eventCodeToKey(event.code);
        if (key != KeyboardKey::Unknown) {
          m_game->onKeyUp(key);
        }
        else {
          auto btn = eventCodeToButton(event.code);
          if (btn != GamepadButton::Unknown) {
            m_game->onButtonUp(btn);
          }
        }
      }
      break;
    }
    case EV_REL: {
      if (event.code == REL_X) {
        m_mouseDelta[0] += event.value;
      }
      if (event.code == REL_Y) {
        m_mouseDelta[1] += event.value;
      }
      break;
    }
    case EV_ABS: {
      const float scaleFactor = 1.f / 32000;

      switch (event.code) {
        case ABS_X:
          m_currentLeftStickPos[0] = scaleFactor * event.value;
          break;
        case ABS_Y:
          m_currentLeftStickPos[1] = scaleFactor * event.value;
          break;
        case ABS_RX:
          m_currentRightStickPos[0] = scaleFactor * event.value;
          break;
        case ABS_RY:
          m_currentRightStickPos[1] = scaleFactor * event.value;
          break;
        case ABS_HAT0X: {
          switch (event.value) {
            case 0:
              m_game->onButtonUp(GamepadButton::Left);
              m_game->onButtonUp(GamepadButton::Right);
              break;
            case -1:
              m_game->onButtonDown(GamepadButton::Left);
              break;
            case 1:
              m_game->onButtonDown(GamepadButton::Right);
              break;
            default: break;
          }
          break;
        }
        case ABS_HAT0Y: {
          switch (event.value) {
            case 0:
              m_game->onButtonUp(GamepadButton::Up);
              m_game->onButtonUp(GamepadButton::Down);
              break;
            case -1:
              m_game->onButtonDown(GamepadButton::Up);
              break;
            case 1:
              m_game->onButtonDown(GamepadButton::Down);
              break;
            default: break;
          }
          break;
        }
        default: break;
      }
      break;
    }
    default: break;
  }
}

void Application::applyInputDeltas()
{
  if (m_mouseDelta != Vec2f{ 0, 0 }) {
    Vec2f delta{ m_mouseDelta[0] / SCREEN_W, m_mouseDelta[1] / SCREEN_H };
    auto pos = m_prevMousePos + delta;

    m_game->onMouseMove(pos, delta);
    m_prevMousePos = pos;
  }

  m_mouseDelta = {};

  m_game->onLeftStickMove(m_currentLeftStickPos);
  m_game->onRightStickMove(m_currentRightStickPos);
}

void Application::pollEvents()
{
  for (size_t i = 0; i < m_devices.size(); ++i) {
    if (m_devices[i].device == nullptr) {
      continue;
    }
    auto& device = m_devices[i];

    input_event event;
    while (libevdev_next_event(device.device, LIBEVDEV_READ_FLAG_NORMAL, &event)
      == LIBEVDEV_READ_STATUS_SUCCESS) {

      handleEvent(event);
    }
  }
}

void Application::run()
{
  FrameRateLimiter frameRateLimiter{TICKS_PER_SECOND};

  while (true) {
    pollEvents();
    applyInputDeltas();

    if (!m_game->update()) {
      break;
    }

    frameRateLimiter.wait();
  }
}

void Application::cleanUp()
{
}

Application::~Application()
{
  cleanUp();
}

} // namespace
} // namespace lithic3d

int main()
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
