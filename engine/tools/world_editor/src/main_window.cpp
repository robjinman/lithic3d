#include <sstream>
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <lithic3d/lithic3d.hpp>

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

enum MenuItems
{
  MNU_OPEN = wxID_HIGHEST + 1
};

class MainWindow : public wxFrame
{
  public:
    explicit MainWindow(const wxString& title);

    ~MainWindow() override;

  private:
    void constructMenu();
    void constructLeftPanel();
    void constructRightPanel();
    void constructPrefabsPanel();
    void populatePrefabsPanel();
    EntityId constructLight();
    void startEngine();
    void constructCursor();
    void positionCursor();
    void processKeyboardInput();

    void onOpen(wxCommandEvent& e);
    void onExit(wxCommandEvent& e);
    void onAbout(wxCommandEvent& e);
    void onClose(wxCloseEvent& e);
    void onTick(wxTimerEvent& e);

    void onCanvasResize(wxSizeEvent& e);
    void onCanvasKeyDown(wxKeyEvent& e);
    void onCanvasKeyUp(wxKeyEvent& e);
    void onCanvasLeftMouseBtnDown(wxMouseEvent& e);
    void onCanvasLeftMouseBtnUp(wxMouseEvent& e);
    void onCanvasMouseMove(wxMouseEvent& e);

    wxSplitterWindow* m_splitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxNotebook* m_rightPanel = nullptr;
    wxPanel* m_prefabsPanel = nullptr;
    wxPanel* m_canvas = nullptr;
    fs::path m_projectRoot;
    wxTimer* m_timer = nullptr;
    GameConfig m_config;
    EnginePtr m_engine;
    InputState m_inputState;
    EntityId m_cursorId = NULL_ENTITY_ID;
};

MainWindow::MainWindow(const wxString& title)
  : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition)
{
  Bind(wxEVT_MENU, &MainWindow::onOpen, this, MNU_OPEN);
  Bind(wxEVT_MENU, &MainWindow::onExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &MainWindow::onAbout, this, wxID_ABOUT);
  Bind(wxEVT_CLOSE_WINDOW, &MainWindow::onClose, this);

  constructMenu();

  m_vbox = new wxBoxSizer(wxVERTICAL);
  SetSizer(m_vbox);

  SetAutoLayout(true);

  m_splitter = new wxSplitterWindow(this);
  m_splitter->SetMinimumPaneSize(300);

  m_vbox->Add(m_splitter, 1, wxEXPAND, 0);

  constructLeftPanel();
  constructRightPanel();

  m_splitter->SplitVertically(m_leftPanel, m_rightPanel, 10000);

  CreateStatusBar();
  SetStatusText(wxEmptyString);

  Maximize();
}

void MainWindow::onOpen(wxCommandEvent&)
{
  auto dialog = new wxDirDialog{this, "Open directory"};

  if (dialog->ShowModal() != wxID_OK) {
    return;
  }

  m_projectRoot = dialog->GetPath().ToStdString();

  startEngine();

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);

  constructCursor();

  constructPrefabsPanel();
  populatePrefabsPanel();

  constructLight();

  m_canvas->Bind(wxEVT_SIZE, &MainWindow::onCanvasResize, this);
  m_canvas->Bind(wxEVT_KEY_DOWN, &MainWindow::onCanvasKeyDown, this);
  m_canvas->Bind(wxEVT_KEY_UP, &MainWindow::onCanvasKeyUp, this);
  m_canvas->Bind(wxEVT_LEFT_DOWN, &MainWindow::onCanvasLeftMouseBtnDown, this);
  m_canvas->Bind(wxEVT_LEFT_UP, &MainWindow::onCanvasLeftMouseBtnUp, this);
  m_canvas->Bind(wxEVT_MOTION, &MainWindow::onCanvasMouseMove, this);

  Bind(wxEVT_TIMER, &MainWindow::onTick, this);
}

void MainWindow::constructCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  m_cursorId = m_engine->ecs().idGen().getNewEntityId();
  m_engine->ecs().componentStore().allocate<DSpatial, DModel>(m_cursorId);

  Vec3f size{ 0.5f, 0.5f, 0.5f };

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();
  spatial.aabb = {
    .min = -size * 0.5f,
    .max = size * 0.5f
  };

  sysSpatial.addEntity(m_cursorId, spatial);

  auto mesh = render::cuboid(size, { 1.f, 1.f });
  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      render::BufferUsage::AttrPosition,
      render::BufferUsage::AttrNormal,
      render::BufferUsage::AttrTexCoord // TODO: Remove
    },
    .flags{}
  };

  auto material = std::make_unique<render::Material>();
  material->colour = { 1.f, 0.f, 0.f, 0.5f };
  material->featureSet = {
    .flags = bitflag(render::MaterialFeatures::HasTransparency)
  };

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_engine->renderResourceLoader().loadMeshAsync(std::move(mesh)).wait(),
      .material = m_engine->renderResourceLoader().loadMaterialAsync(std::move(material)).wait(),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  auto render = std::make_unique<DModel>();
  render->model = m_engine->modelLoader().loadModelAsync(std::move(model)).wait();

  sysRender3d.addEntity(m_cursorId, std::move(render));
}

void MainWindow::startEngine()
{
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

  auto windowDelegate = createWindowDelegate(m_canvas->GetHandle());
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
}

void MainWindow::positionCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  auto camPos = sysRender3d.camera().getPosition();
  auto camDir = sysRender3d.camera().getDirection();

  float distance = metresToWorldUnits(3.f);

  sysSpatial.setLocalTransform(m_cursorId, translationMatrix4x4(camPos + camDir * distance));
}

void MainWindow::onClose(wxCloseEvent& e)
{
  e.Skip();
}

EntityId MainWindow::constructLight()
{
  auto id = m_engine->ecs().idGen().getNewEntityId();
  m_engine->ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  DSpatial spatial{
    .transform = {},
    .parent = m_engine->ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_engine->ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 0.9f, 0.9f };
  light->ambient = 0.4f;
  light->specular = 0.9f;

  m_engine->ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

void MainWindow::processKeyboardInput()
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

void MainWindow::onCanvasKeyDown(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_inputState.keysPressed.insert(key);
}

void MainWindow::onCanvasKeyUp(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_inputState.keysPressed.erase(key);
}

void MainWindow::onCanvasResize(wxSizeEvent& e)
{
  e.Skip();

  m_engine->onWindowResize(e.GetSize().GetWidth(), e.GetSize().GetHeight());

#ifdef PLATFORM_OSX
  osxResizeMetalLayer(GetHandle(), e.GetSize().GetWidth(), e.GetSize().GetHeight());
#endif
}

void MainWindow::onCanvasLeftMouseBtnDown(wxMouseEvent& e)
{
  e.Skip();

  m_canvas->SetFocus();

  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void MainWindow::onCanvasLeftMouseBtnUp(wxMouseEvent& e)
{
  e.Skip();

  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void MainWindow::onCanvasMouseMove(wxMouseEvent& e)
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

void MainWindow::onTick(wxTimerEvent&)
{
  ASSERT(m_engine, "Engine not started");

  processKeyboardInput();
  positionCursor();
  m_engine->update(m_inputState);
  m_engine->worldGrid().update(m_engine->ecs().system<SysRender3d>().camera().getPosition());
}

void MainWindow::constructMenu()
{
  wxMenu* mnuFile = new wxMenu;
  auto itmOpen = new wxMenuItem(mnuFile, MNU_OPEN, "Open");
  mnuFile->Append(itmOpen);
  mnuFile->Append(wxID_EXIT);

  wxMenu* mnuHelp = new wxMenu;
  mnuHelp->Append(wxID_ABOUT);

  wxMenuBar* menuBar = new wxMenuBar;
  menuBar->Append(mnuFile, wxGetTranslation("&File"));
  menuBar->Append(mnuHelp, wxGetTranslation("&Help"));

  SetMenuBar(menuBar);
}

void MainWindow::constructLeftPanel()
{
  assert(m_splitter);

  m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_canvas = new wxPanel(m_leftPanel);
  vbox->Add(m_canvas, 1, wxEXPAND | wxALL, 5);
  m_leftPanel->SetSizer(vbox);
  m_leftPanel->SetCanFocus(true);
}

void MainWindow::constructPrefabsPanel()
{
  assert(m_rightPanel);

  m_prefabsPanel = new wxPanel(m_rightPanel);
  m_rightPanel->AddPage(m_prefabsPanel, "Prefabs");
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_prefabsPanel->SetSizer(vbox);
}

void MainWindow::constructRightPanel()
{
  assert(m_splitter);
  m_rightPanel = new wxNotebook(m_splitter, wxID_ANY);
}

void MainWindow::populatePrefabsPanel()
{
  assert(m_prefabsPanel);

  fs::directory_iterator it{m_projectRoot / "data/prefabs"};
  auto listBox = new wxListBox{m_prefabsPanel, wxID_ANY};
  m_prefabsPanel->GetSizer()->Add(listBox, 1, wxEXPAND | wxALL);

  size_t i = 0;
  auto end = fs::end(it);
  for (; it != end; ++it) {
    if (it->is_regular_file()) {
      auto xmlPrefab = parseXml(readBinaryFile(it->path()));
      listBox->Insert(xmlPrefab->attribute("name"), i++);
    }
  }
}

void MainWindow::onExit(wxCommandEvent&)
{
  Close();
}

void MainWindow::onAbout(wxCommandEvent&)
{
  std::stringstream ss;
  ss << "Lithic3D World Editor." << std::endl << std::endl;
  ss << "Copyright Freehold Apps Ltd 2025 - 2026. All rights reserved.";

  wxMessageBox(wxGetTranslation(ss.str()), "Lithic3D World Editor", wxOK | wxICON_INFORMATION);
}

MainWindow::~MainWindow()
{
  if (m_engine) {
    m_engine->resourceManager().deactivate();
  }
  if (m_timer) {
    m_timer->Stop();
  }
}

} // namespace

wxFrame* createMainWindow(const wxString& title)
{
  return new MainWindow(title);
}
