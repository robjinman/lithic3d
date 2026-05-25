#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>
#include <iostream>

namespace lithic3d
{

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

} // namespace lithic3d

namespace fs = std::filesystem;
using namespace lithic3d;

namespace
{

class EditorCoreImpl : public EditorCore
{
  public:
    EditorCoreImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate);

    const GameConfig& config() const override;
    Engine& engine() const override;

    float getCursorDistance() const override;
    Vec3f getCursorRotation() override;
    const Vec3f& getCursorScale() const override;
    Mat4x4f getCursorTransform() const override;

    void setCursorDistance(float metres) override;
    void setCursorRotation(const Vec3f& ori) override;
    void setCursorScale(const Vec3f& scale) override;
    void setCursorTransform(const Mat4x4f& transform) override;

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

    const lithic3d::InputState& inputState() const override;

    std::vector<std::string> listPrefabs() const override;

    void onCanvasResize(uint32_t w, uint32_t h) override;

    void update() override;

    void listen(Event event, const Callback& callback) override;

    ~EditorCoreImpl() override;

  private:
    EntityId constructLight();
    void createEngine();
    void constructCursor();
    void positionCursor();
    void setCursorRotation(const Mat3x3f& rot);
    void raiseEvent(Event event);
    void cleanUp();

    fs::path m_projectRoot;
    WindowDelegate& m_windowDelegate;
    EnginePtr m_engine;
    GameConfig m_config;
    InputState m_inputState;
    EntityId m_cursorId = NULL_ENTITY_ID;
    std::map<Event, std::vector<Callback>> m_eventHandlers;
  
    Vec3f m_activeScale = Vec3f{ 1.f, 1.f, 1.f } * WORLD_UNITS_PER_METRE;
    Mat3x3f m_activeRotation = identityMatrix<3>();
    float m_activeTranslation = metresToWorldUnits(10.f);
};

EditorCoreImpl::EditorCoreImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate)
  : m_projectRoot(projectRoot)
  , m_windowDelegate(windowDelegate)
{
  createEngine();

  try {
    constructLight();
    constructCursor();
  }
  catch (...) {
    cleanUp();
    throw;
  }
}

const GameConfig& EditorCoreImpl::config() const
{
  return m_config;
}

Engine& EditorCoreImpl::engine() const
{
  return *m_engine;
}

void EditorCoreImpl::cleanUp()
{
  m_engine->resourceManager().deactivate();
}

void EditorCoreImpl::raiseEvent(Event event)
{
  for (auto& cb : m_eventHandlers[event]) {
    cb();
  }
}

void EditorCoreImpl::listen(Event event, const Callback& callback)
{
  m_eventHandlers[event].push_back(callback);
}

float EditorCoreImpl::getCursorDistance() const
{
  return m_activeTranslation;
}

Vec3f EditorCoreImpl::getCursorRotation()
{
  return eulerAnglesFromMatrix(m_activeRotation);
}

const Vec3f& EditorCoreImpl::getCursorScale() const
{
  return m_activeScale;
}

Mat4x4f EditorCoreImpl::getCursorTransform() const
{
  auto& camera = m_engine->ecs().system<SysRender3d>().camera();
  auto translation = camera.getPosition() + camera.getDirection() * m_activeTranslation;
  return translationMatrix4x4(translation) * rotationMatrix4x4(m_activeRotation) *
    scaleMatrix4x4(m_activeScale);
}

std::vector<std::string> EditorCoreImpl::listPrefabs() const
{
  fs::directory_iterator it{m_projectRoot / "data/prefabs"};

  std::vector<std::string> prefabNames;

  auto end = fs::end(it);
  for (; it != end; ++it) {
    if (it->is_regular_file()) {
      prefabNames.push_back(it->path().stem().string());
    }
  }

  return prefabNames;
}

void EditorCoreImpl::setCursorTransform(const Mat4x4f& m)
{
  auto rotationScale = getRotation3x3(m);
  Mat3x3f rotation;
  Vec3f scale;
  decomposeRotationScale(rotationScale, rotation, scale);

  m_activeRotation = rotation;
  m_activeScale = scale;

  raiseEvent(Event::CursorMove);
}

void EditorCoreImpl::setCursorDistance(float metres)
{
  m_activeTranslation = metresToWorldUnits(metres);
}

void EditorCoreImpl::setCursorRotation(const Vec3f& ori)
{
  setCursorRotation(rotationMatrix3x3(ori));
}

void EditorCoreImpl::setCursorRotation(const Mat3x3f& rot)
{
  m_activeRotation = rot;
}

void EditorCoreImpl::setCursorScale(const Vec3f& scale)
{
  m_activeScale = scale;
}

void EditorCoreImpl::constructCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  Vec3f postScaleSize = metresToWorldUnits(Vec3f{ 1.f, 1.f, 1.f });
  Vec3f preScaleSize = postScaleSize / m_activeScale;

  m_cursorId = m_engine->ecs().idGen().getNewEntityId();
  m_engine->ecs().componentStore().allocate<DSpatial, DModel>(m_cursorId);

  DSpatial spatial{
    .transform = identityMatrix<4>(),
    .parent = m_engine->worldGrid().root(),
    .enabled = true,
    .aabb = {
      .min = -preScaleSize * 0.5f,
      .max = preScaleSize * 0.5f
    }
  };

  sysSpatial.addEntity(m_cursorId, spatial);

  auto mesh = render::cuboid(preScaleSize, { 1.f, 1.f });
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

void EditorCoreImpl::createEngine()
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

  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
  auto audioSystem = createAudioSystem(m_config.paths.soundsDir, *logger);
  auto resourceManager = createResourceManager(*logger);
  auto renderer = createRenderer(m_windowDelegate, *resourceManager, m_config.paths, *logger, {});

  logger->info("Compiling shaders...");

  auto manifest = m_config.paths.shaderManifest.read();
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = lithic3d::createEngine(m_config, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));
}

void EditorCoreImpl::positionCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  auto& camera = sysRender3d.camera();
  auto camPos = camera.getPosition();
  auto camDir = camera.getDirection();

  Vec3f translation = camPos + camDir * m_activeTranslation;

  auto transform = translationMatrix4x4(translation) * rotationMatrix4x4(m_activeRotation) *
    scaleMatrix4x4(m_activeScale);

  sysSpatial.setLocalTransform(m_cursorId, transform);
}

EntityId EditorCoreImpl::constructLight()
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

void EditorCoreImpl::onCanvasResize(uint32_t w, uint32_t h)
{
  m_engine->onWindowResize(w, h);
}

void EditorCoreImpl::onKeyDown(KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);
}

void EditorCoreImpl::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void EditorCoreImpl::onMouseLeftBtnDown()
{
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void EditorCoreImpl::onMouseLeftBtnUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void EditorCoreImpl::onMouseMove(float x, float y)
{
  m_inputState.mouseX = x;
  m_inputState.mouseY = y;
}

const lithic3d::InputState& EditorCoreImpl::inputState() const
{
  return m_inputState;
}

void EditorCoreImpl::update()
{
  ASSERT(m_engine, "Engine not started");

  m_engine->update(m_inputState);
  positionCursor();
}

EditorCoreImpl::~EditorCoreImpl()
{
  cleanUp();
}

} // namespace

EditorCorePtr createEditorCore(const fs::path& projectRoot, WindowDelegate& windowDelegate)
{
  return std::make_unique<EditorCoreImpl>(projectRoot, windowDelegate);
}
