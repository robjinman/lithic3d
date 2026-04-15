#include <lithic3d/lithic3d.hpp>
#include "canvas.hpp"

namespace fs = std::filesystem;

#ifdef PLATFORM_OSX
void osxResizeMetalLayer(WXWidget window, int width, int height);
#endif

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window);

namespace lithic3d
{

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

} // namespace lithic3d

using namespace lithic3d;

namespace
{

class CanvasImpl : public Canvas
{
  public:
    explicit CanvasImpl(wxWindow* parent);

    void enable() override;
    void disable() override;
    void startEngine(const fs::path& projectRoot) override;
    Engine& engine() const override;

    ~CanvasImpl() override;

  private:
    wxPoint getCursorPos() const;

    void onResize(wxSizeEvent& e);
    void onKeyDown(wxKeyEvent& e);
    void onKeyUp(wxKeyEvent& e);
    void onLeftMouseBtnDown(wxMouseEvent& e);
    void onLeftMouseBtnUp(wxMouseEvent& e);
    void onMouseMove(wxMouseEvent& e);
    void onTick(wxTimerEvent& e);
    void onPaint(wxPaintEvent& e);

    void processKeyboardInput();

    fs::path m_projectRoot;
    bool m_disabled = false;
    wxTimer* m_timer = nullptr;
    GameConfig m_config;
    EnginePtr m_engine;
    InputState m_inputState;
};

CanvasImpl::CanvasImpl(wxWindow* parent)
  : Canvas(parent)
{
  Bind(wxEVT_PAINT, &CanvasImpl::onPaint, this);
  Bind(wxEVT_SIZE, &CanvasImpl::onResize, this);
  Bind(wxEVT_KEY_DOWN, &CanvasImpl::onKeyDown, this);
  Bind(wxEVT_KEY_UP, &CanvasImpl::onKeyUp, this);
  Bind(wxEVT_LEFT_DOWN, &CanvasImpl::onLeftMouseBtnDown, this);
  Bind(wxEVT_LEFT_UP, &CanvasImpl::onLeftMouseBtnUp, this);
  Bind(wxEVT_MOTION, &CanvasImpl::onMouseMove, this);
  Bind(wxEVT_TIMER, &CanvasImpl::onTick, this);
}

void CanvasImpl::startEngine(const fs::path& projectRoot)
{
  m_projectRoot = projectRoot;

  auto platformPaths = createPlatformPaths("lithic3d_world_editor", "freeholdapps");
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));

  m_config.paths = GameDataPaths{
    .shadersDir = fileSystem->directory(m_projectRoot / "data/shaders"),
    .texturesDir = fileSystem->directory(m_projectRoot / "data/textures"),
    .soundsDir = fileSystem->directory(m_projectRoot / "data/sounds"),
    .prefabsDir = fileSystem->directory(m_projectRoot / "data/prefabs"),
    .modelsDir = fileSystem->directory(m_projectRoot / "data/models"),
    .worldsDir = fileSystem->directory(m_projectRoot / "data/worlds"),
    .shaderManifest = FilePath{
      .directory = fileSystem->directory(m_projectRoot),
      .subpath = "data/shaders.xml"
    }
  };
  m_config.features = {
    .terrain = true
  };
  m_config.drawDistance = 1000.f; // In metres

  auto windowDelegate = createWindowDelegate(GetHandle());
  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto audioSystem = createAudioSystem(m_config.paths.soundsDir, *logger);
  auto resourceManager = createResourceManager(*logger);
  auto renderer = createRenderer(std::move(windowDelegate), *resourceManager, m_config.paths,
    *logger, {});

  logger->info("Compiling shaders...");

  auto manifest = m_config.paths.shaderManifest.read();
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = createEngine(m_config, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));

  Vec3f initialPos = metresToWorldUnits(Vec3f{ 11305.0, 30.0, 9199.0 });
  m_engine->ecs().system<SysRender3d>().camera().setPosition(initialPos);

  m_engine->worldGrid().update(initialPos);
  m_engine->worldGrid().wait();

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);
}

Engine& CanvasImpl::engine() const
{
  ASSERT(m_engine, "Engine not started");
  return *m_engine;
}

void CanvasImpl::onPaint(wxPaintEvent&)
{
}

KeyboardKey mapToLithic3dKey(int code)
{
  if (code >= 'A' && code <= 'Z') {
    return static_cast<KeyboardKey>(code);
  }
  if (code >= '0' && code <= '9') {
    return static_cast<KeyboardKey>(code);
  }
  switch (code) {
    case WXK_SPACE: return KeyboardKey::Space;
    case WXK_ESCAPE: return KeyboardKey::Escape;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER: return KeyboardKey::Enter;
    case WXK_BACK: return KeyboardKey::Backspace;
    case WXK_F1: return KeyboardKey::F1;
    case WXK_F2: return KeyboardKey::F2;
    case WXK_F3: return KeyboardKey::F3;
    case WXK_F4: return KeyboardKey::F4;
    case WXK_F5: return KeyboardKey::F5;
    case WXK_F6: return KeyboardKey::F6;
    case WXK_F7: return KeyboardKey::F7;
    case WXK_F8: return KeyboardKey::F8;
    case WXK_F9: return KeyboardKey::F9;
    case WXK_F10: return KeyboardKey::F10;
    case WXK_F11: return KeyboardKey::F11;
    case WXK_F12: return KeyboardKey::F12;
    case WXK_RIGHT: return KeyboardKey::Right;
    case WXK_LEFT: return KeyboardKey::Left;
    case WXK_DOWN: return KeyboardKey::Down;
    case WXK_UP: return KeyboardKey::Up;
    default: return KeyboardKey::Unknown;
  }
}

void CanvasImpl::onKeyDown(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_inputState.keysPressed.insert(key);
}

void CanvasImpl::onKeyUp(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_inputState.keysPressed.erase(key);
}

void CanvasImpl::processKeyboardInput()
{
  auto& camera = m_engine->ecs().system<SysRender3d>().camera();

  float metresPerSecond = 6.f;
  float speed = metresToWorldUnits(metresPerSecond) / TICKS_PER_SECOND;
  const auto forward = camera.getDirection();
  const auto right = forward.cross(Vec3f{0, 1, 0});
  Vec3f direction;

  if (m_inputState.keysPressed.contains(KeyboardKey::W)) {
    direction += forward;
  }
  if (m_inputState.keysPressed.contains(KeyboardKey::S)) {
    direction -= forward;
  }
  if (m_inputState.keysPressed.contains(KeyboardKey::D)) {
    direction += right;
  }
  if (m_inputState.keysPressed.contains(KeyboardKey::A)) {
    direction -= right;
  }
  if (m_inputState.keysPressed.contains(KeyboardKey::Q)) {
    direction += { 0, -1, 0 };
  }
  if (m_inputState.keysPressed.contains(KeyboardKey::E)) {
    direction += { 0, 1, 0 };
  }

  if (direction != Vec3f{}) {
    direction = direction.normalise();
    auto delta = direction * speed;

    camera.translate(delta);
  }
}

void CanvasImpl::onTick(wxTimerEvent&)
{
  ASSERT(m_engine, "Engine not started");

  processKeyboardInput();
  m_engine->update(m_inputState);
  m_engine->worldGrid().update(m_engine->ecs().system<SysRender3d>().camera().getPosition());
}

void CanvasImpl::onResize(wxSizeEvent& e)
{
  e.Skip();

  if (!m_engine) {
    return;
  }

  m_engine->onWindowResize(e.GetSize().GetWidth(), e.GetSize().GetHeight());

#ifdef PLATFORM_OSX
  osxResizeMetalLayer(GetHandle(), e.GetSize().GetWidth(), e.GetSize().GetHeight());
#endif
}

wxPoint CanvasImpl::getCursorPos() const
{
  auto pt = wxGetMousePosition();
  return pt - GetScreenPosition();
}

void CanvasImpl::onLeftMouseBtnDown(wxMouseEvent& e)
{
  e.Skip();

  SetFocus();

  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void CanvasImpl::onLeftMouseBtnUp(wxMouseEvent& e)
{
  e.Skip();

  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void CanvasImpl::onMouseMove(wxMouseEvent& e)
{
  e.Skip();

  if (!m_engine) {
    return;
  }

  float x = static_cast<float>(e.GetX()) / GetClientSize().GetWidth();
  float y = static_cast<float>(e.GetY()) / GetClientSize().GetHeight();

  if (e.Dragging() && m_inputState.mouseButtonsPressed.contains(MouseButton::Left)) {
    float speed = 3.f;
    float dx = x - m_inputState.mouseX;
    float dy = y - m_inputState.mouseY;

    auto& camera = m_engine->ecs().system<SysRender3d>().camera();
    camera.rotate(-dy * speed, dx * speed);
  }

  m_inputState.mouseX = x;
  m_inputState.mouseY = y;
}

void CanvasImpl::disable()
{
  m_disabled = true;
  Disable();
}

void CanvasImpl::enable()
{
  m_disabled = false;
  Enable();
}

CanvasImpl::~CanvasImpl()
{
  if (m_engine) {
    m_engine->resourceManager().deactivate();
  }
  if (m_timer) {
    m_timer->Stop();
  }
}

} // namespace

Canvas* createCanvas(wxWindow* parent)
{
  return new CanvasImpl(parent);
}
