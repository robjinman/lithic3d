#include "canvas.hpp"
#include <lithic3d/lithic3d.hpp>

namespace lithic3d
{

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

} // namespace lithic3d

using namespace lithic3d;

WindowDelegatePtr createWindowDelegate(WXWidget window);
#ifdef PLATFORM_OSX
void osxResizeMetalLayer(WXWidget window, int width, int height);
#endif

namespace
{

class CanvasImpl : public Canvas
{
  public:
    explicit CanvasImpl(wxWindow* parent);

    void startEngine(const std::filesystem::path& projectPath) override;

    void enable() override;
    void disable() override;

    ~CanvasImpl() override;

  private:
    wxPoint getCursorPos() const;

    void onResize(wxSizeEvent& e);
    void onKeyPress(wxKeyEvent& e);
    void onLeftMouseBtnDown(wxMouseEvent& e);
    void onLeftMouseBtnUp(wxMouseEvent& e);
    void onMouseMove(wxMouseEvent& e);
    void onTick(wxTimerEvent& e);
    void onPaint(wxPaintEvent& e);

    // TODO: Remove
    //FactoryPtr m_factory;
    //EntityId m_cube = NULL_ENTITY_ID;

    //EntityId constructCube();
    EntityId constructLight();
    //void rotateCube();

    GameConfig m_config;
    EnginePtr m_engine;
    bool m_disabled = false;
    wxTimer* m_timer = nullptr;
    bool m_mouseDown = false;
    wxRect m_selectionRect;
};

CanvasImpl::CanvasImpl(wxWindow* parent)
  : Canvas(parent)
{
  Bind(wxEVT_PAINT, &CanvasImpl::onPaint, this);
  Bind(wxEVT_SIZE, &CanvasImpl::onResize, this);
  Bind(wxEVT_KEY_DOWN, &CanvasImpl::onKeyPress, this);
  Bind(wxEVT_LEFT_DOWN, &CanvasImpl::onLeftMouseBtnDown, this);
  Bind(wxEVT_LEFT_UP, &CanvasImpl::onLeftMouseBtnUp, this);
  Bind(wxEVT_MOTION, &CanvasImpl::onMouseMove, this);
  Bind(wxEVT_TIMER, &CanvasImpl::onTick, this);

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);
}

void CanvasImpl::onPaint(wxPaintEvent& e)
{
}

void CanvasImpl::startEngine(const std::filesystem::path& projectPath)
{
  auto platformPaths = createPlatformPaths("lithic3d_world_editor", "freeholdapps");
  auto fileSystem = createDefaultFileSystem(std::move(platformPaths));

  m_config.paths = GameDataPaths{
    .shadersDir = fileSystem->directory(projectPath / "data/shaders"),
    .texturesDir = fileSystem->directory(projectPath / "data/textures"),
    .soundsDir = fileSystem->directory(projectPath / "data/sounds"),
    .prefabsDir = fileSystem->directory(projectPath / "data/prefabs"),
    .modelsDir = fileSystem->directory(projectPath / "data/models"),
    .worldsDir = fileSystem->directory(projectPath / "data/worlds"),
    .shaderManifest = FilePath{
      .directory = fileSystem->directory(projectPath),
      .subpath = "data/shaders.xml"
    }
  };
  m_config.features = {
    .terrain = true
  };
  m_config.drawDistance = 100.f; // In metres

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

  Vec3f initialPos{ 11305.0, 30.0, 9199.0 };
  m_engine->ecs().system<SysRender3d>().camera().setPosition(metresToWorldUnits(initialPos));

  // TODO
  //m_factory = createFactory(m_engine->ecs(), m_engine->modelLoader(),
  //  m_engine->renderResourceLoader());

  constructLight();
  //m_cube = constructCube();
}

//EntityId CanvasImpl::constructCube()
//{
//  auto material = m_factory->createMaterialAsync("textures/bricks.png").wait();
//  return m_factory->createStaticCuboid({ 1.f, 1.f, 1.f }, material, { 1.f, 1.f }, 0.2f, 0.4f);
//}

EntityId CanvasImpl::constructLight()
{
  auto id = m_engine->ecs().idGen().getNewEntityId();
  m_engine->ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  DSpatial spatial{
    .transform = {},
    .parent = m_engine->ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine->ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 0.9f, 0.9f };
  light->ambient = 0.4f;
  light->specular = 0.9f;

  m_engine->ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

//void CanvasImpl::rotateCube()
//{
//  float a = (2 * PIf / 360.f) * (m_engine->currentTick() % 360);
//  float b = (2 * PIf / 720.f) * (m_engine->currentTick() % 720);

//  m_engine->ecs().system<SysSpatial>().setLocalTransform(m_cube,
//    createTransform(Vec3f{ 0.f, 0.f, -5.f }, { b, a, 0.f }));
//}

void CanvasImpl::onTick(wxTimerEvent& e)
{
  if (m_engine) {
    //rotateCube();
    m_engine->update({});
    m_engine->worldGrid().update(m_engine->ecs().system<SysRender3d>().camera().getPosition());
  }
}

void CanvasImpl::onResize(wxSizeEvent& e)
{
  e.Skip();

  // TODO: On OSX, resize metal layer

  if (m_engine != nullptr) {
    m_engine->onWindowResize(e.GetSize().GetWidth(), e.GetSize().GetHeight());
  }

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

  m_mouseDown = true;
  m_selectionRect = wxRect(e.GetPosition(), wxSize(0, 0));
}

void CanvasImpl::onLeftMouseBtnUp(wxMouseEvent& e)
{
  e.Skip();

  m_mouseDown = false;

  auto selectionSizeAboveThreshold = [](wxSize sz) {
    return sz.x * sz.x + sz.y * sz.y >= 64;
  };

  if (selectionSizeAboveThreshold(m_selectionRect.GetSize())) {
    int x0 = m_selectionRect.x;
    int y0 = m_selectionRect.y;
    int x1 = x0 + m_selectionRect.width;
    int y1 = y0 + m_selectionRect.height;

    // TODO
  }
}

void CanvasImpl::onMouseMove(wxMouseEvent& e)
{
  e.Skip();

  if (m_mouseDown) {
    wxPoint p = wxGetMousePosition() - GetScreenPosition();
    wxPoint sz_p = p - m_selectionRect.GetTopLeft();
    wxSize sz(sz_p.x, sz_p.y);

    wxSize winSz = GetClientSize();
    float aspect = static_cast<float>(winSz.x) / static_cast<float>(winSz.y);
    sz.y = sz.x / aspect;

    m_selectionRect.SetSize(sz);
  }
}

void CanvasImpl::onKeyPress(wxKeyEvent& e)
{
  e.Skip();

  auto key = e.GetKeyCode();

  // TODO
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
  m_timer->Stop();
  m_engine->resourceManager().deactivate();
}

} // namespace

Canvas* createCanvas(wxWindow* parent)
{
  return new CanvasImpl(parent);
}
