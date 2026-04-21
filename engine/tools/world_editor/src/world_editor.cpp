#include "world_editor.hpp"
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

class WorldEditorImpl : public WorldEditor
{
  public:
    WorldEditorImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate);

    std::vector<std::string> listPrefabs() const override;
    void instantiatePrefabAtCursor(const std::string& name) override;
    void update() override;
    void onKeyDown(lithic3d::KeyboardKey key) override;
    void onKeyUp(lithic3d::KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;
    void onCanvasResize(uint32_t w, uint32_t h) override;

    ~WorldEditorImpl() override;

  private:
    EntityId constructLight();
    void createEngine();
    void constructCursor();
    void positionCursor();
    void processKeyboardInput();

    fs::path m_projectRoot;
    WindowDelegate& m_windowDelegate;
    EnginePtr m_engine;
    GameConfig m_config;
    InputState m_inputState;
    EntityId m_cursorId = NULL_ENTITY_ID;
    std::map<std::string, ResourceHandle> m_prefabs;
    EntityId m_activeEntity = NULL_ENTITY_ID;
};

WorldEditorImpl::WorldEditorImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate)
  : m_projectRoot(projectRoot)
  , m_windowDelegate(windowDelegate)
{
  createEngine();
  constructLight();
  constructCursor();
}

std::vector<std::string> WorldEditorImpl::listPrefabs() const
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

void WorldEditorImpl::instantiatePrefabAtCursor(const std::string& name)
{
  auto& factory = m_engine->entityFactory();
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();

  ResourceHandle handle;
  auto i = m_prefabs.find(name);
  if (i == m_prefabs.end()) {
    handle = factory.loadPrefabAsync(name).wait();
    m_prefabs.insert(std::make_pair(name, handle));
  }
  else {
    handle = i->second;
  }

  Mat4x4f transform = sysSpatial.getGlobalTransform(m_cursorId);
  m_activeEntity = factory.constructEntity(name, transform);
}

void WorldEditorImpl::constructCursor()
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

void WorldEditorImpl::createEngine()
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
  auto renderer = createRenderer(m_windowDelegate, *resourceManager, m_config.paths,
    *logger, {});

  logger->info("Compiling shaders...");

  auto manifest = m_config.paths.shaderManifest.read();
  auto specs = parseShaderManifest(manifest, *logger);
  for (auto& spec : specs) {
    renderer->compileShader(spec);
  }

  logger->info("Finished compiling shaders");

  m_engine = lithic3d::createEngine(m_config, std::move(resourceManager), std::move(renderer),
    std::move(audioSystem), std::move(fileSystem), std::move(logger));

  Vec3f initialPos = metresToWorldUnits(Vec3f{ 11305.0, 30.0, 9199.0 });
  m_engine->ecs().system<SysRender3d>().camera().setPosition(initialPos);

  m_engine->worldGrid().update(initialPos);
  m_engine->worldGrid().wait();
}

void WorldEditorImpl::positionCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  auto camPos = sysRender3d.camera().getPosition();
  auto camDir = sysRender3d.camera().getDirection();

  float distance = metresToWorldUnits(3.f);
  auto transform = translationMatrix4x4(camPos + camDir * distance);

  sysSpatial.setLocalTransform(m_cursorId, transform);

  if (m_activeEntity != NULL_ENTITY_ID) {
    sysSpatial.setLocalTransform(m_activeEntity, transform);
  }
}

EntityId WorldEditorImpl::constructLight()
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

void WorldEditorImpl::processKeyboardInput()
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

void WorldEditorImpl::onKeyDown(lithic3d::KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);
}

void WorldEditorImpl::onKeyUp(lithic3d::KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void WorldEditorImpl::onMouseLeftBtnDown()
{
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void WorldEditorImpl::onMouseLeftBtnUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void WorldEditorImpl::onMouseMove(float x, float y)
{
  if (m_inputState.mouseButtonsPressed.contains(MouseButton::Left)) {
    float speed = 3.f;
    float dx = x - m_inputState.mouseX;
    float dy = y - m_inputState.mouseY;

    auto& camera = m_engine->ecs().system<SysRender3d>().camera();
    camera.rotate(-dy * speed, dx * speed);
  }

  m_inputState.mouseX = x;
  m_inputState.mouseY = y;
}

void WorldEditorImpl::onCanvasResize(uint32_t w, uint32_t h)
{
  m_engine->onWindowResize(w, h);
}

void WorldEditorImpl::update()
{
  ASSERT(m_engine, "Engine not started");

  processKeyboardInput();
  positionCursor();
  m_engine->update(m_inputState);
  m_engine->worldGrid().update(m_engine->ecs().system<SysRender3d>().camera().getPosition());
}

WorldEditorImpl::~WorldEditorImpl()
{
  m_engine->resourceManager().deactivate();
}

} // namespace

WorldEditorPtr createWorldEditor(const fs::path& projectRoot, WindowDelegate& windowDelegate)
{
  return std::make_unique<WorldEditorImpl>(projectRoot, windowDelegate);
}
