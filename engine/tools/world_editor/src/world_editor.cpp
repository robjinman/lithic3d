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

struct SliceState
{
  ResourceHandle handle;
  std::vector<Entity> entities;
  bool dirty = false;
};

struct WorldState
{
  std::map<Vec3i, SliceState> slices;
};

class WorldEditorImpl : public WorldEditor
{
  public:
    WorldEditorImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate);

    std::vector<std::string> listPrefabs() const override;
    void setActivePrefab(const std::string& name) override;
    void instantiateActivePrefab() override;
    void cancelActivePrefab() override;

    void setCursorDistance(float metres) override;
    void setCursorRotation(const Vec3f& ori) override;
    void setCursorScale(float scale) override;

    void update() override;

    void onKeyDown(lithic3d::KeyboardKey key) override;
    void onKeyUp(lithic3d::KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;
    void onCanvasResize(uint32_t w, uint32_t h) override;

    void saveChanges() override;

    ~WorldEditorImpl() override;

  private:
    EntityId constructLight();
    void createEngine();
    void constructCursor();
    void positionCursor();
    void processKeyboardInput();
    Vec2i cellFromPosition(const Vec3f& pos) const;
    void saveChangesToSlice(const Vec3f& index, const SliceState& slice);
    void saveChangesToSlice(const fs::path& cellDirName, const std::string& sliceFileName,
      const SliceState& slice);

    fs::path m_projectRoot;
    WindowDelegate& m_windowDelegate;
    EnginePtr m_engine;
    GameConfig m_config;
    InputState m_inputState;
    EntityId m_cursorId = NULL_ENTITY_ID;
    std::map<std::string, ResourceHandle> m_prefabs;
  
    // TODO: Bundle these into struct?
    EntityId m_activeEntity = NULL_ENTITY_ID;
    std::string m_activeEntityType;
    Vec3f m_activeScale = Vec3f{ 1.f, 1.f, 1.f } * WORLD_UNITS_PER_METRE;
    Vec3f m_activeRotation;
    float m_activeTranslation = metresToWorldUnits(10.f);
  
    WorldState m_state;
};

WorldEditorImpl::WorldEditorImpl(const fs::path& projectRoot, WindowDelegate& windowDelegate)
  : m_projectRoot(projectRoot)
  , m_windowDelegate(windowDelegate)
{
  createEngine();
  constructLight();
  constructCursor();
}

Vec2i WorldEditorImpl::cellFromPosition(const Vec3f& pos) const
{
  ASSERT(m_engine, "Engine not instantiated");

  float cellW = m_engine->worldLoader().worldInfo().cellWidth;
  float cellH = m_engine->worldLoader().worldInfo().cellHeight;

  return {
    static_cast<int>(pos[0] / cellW),
    static_cast<int>(pos[2] / cellH)
  };
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

void WorldEditorImpl::setActivePrefab(const std::string& name)
{
  if (m_activeEntity != NULL_ENTITY_ID) {
    cancelActivePrefab();
  }

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
  m_activeEntityType = name;
}

void WorldEditorImpl::instantiateActivePrefab()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto pos = getTranslation(sysSpatial.getGlobalTransform(m_cursorId));
  auto cell = cellFromPosition(pos);

  int sliceIdx = 1; // TODO
  auto& slice = m_state.slices[{ cell[0], cell[1], sliceIdx }];
  assert(slice.handle.id() != NULL_RESOURCE_ID);
  slice.entities.push_back({
    .id = m_activeEntity,
    .type = m_activeEntityType
  });
  slice.dirty = true;

  m_activeEntity = NULL_ENTITY_ID;
}

void WorldEditorImpl::saveChanges()
{
  for (auto& [ index, slice ] : m_state.slices) {
    if (slice.dirty) {
      saveChangesToSlice(index, slice);
      slice.dirty = false;
    }
  }
}

void WorldEditorImpl::saveChangesToSlice(const fs::path& cellDirName,
  const std::string& sliceFileName, const SliceState& slice)
{
  auto cellDir = m_config.paths.worldsDir->subdirectory(fs::path{"world"} / cellDirName);

  ASSERT(cellDir->fileExists(sliceFileName), "File " << sliceFileName << " doesn't exist");

  auto xmlCellSlice = createXmlNode("cell-slice");
  auto xmlEntities = createXmlNode("entities");
  for (auto& entity : slice.entities) {
    auto xmlEntity = createXmlNode("entity");

    xmlEntity->setAttribute("type", entity.type);

    auto xmlTransform = createXmlNode("transform");
    // TODO

    xmlEntity->addChild(std::move(xmlTransform));
    xmlEntities->addChild(std::move(xmlEntity));
  }
  xmlCellSlice->addChild(std::move(xmlEntities));

  std::stringstream stream;
  xmlCellSlice->write(stream);

  auto xmlString = stream.str();
  cellDir->writeFile(sliceFileName, xmlString.data(), xmlString.size());
}

void WorldEditorImpl::saveChangesToSlice(const Vec3f& index, const SliceState& slice)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << index[0]
    << std::setw(3) << std::setfill('0') << index[1];
  auto cellDirName = ss.str();
  ss.str("");
  ss << std::setw(3) << std::setfill('0') << index[2] << ".xml";
  auto sliceFileName = ss.str();

  saveChangesToSlice(cellDirName, sliceFileName, slice);
}

void WorldEditorImpl::cancelActivePrefab()
{
  if (m_activeEntity != NULL_ENTITY_ID) {
    m_engine->ecs().removeEntity(m_activeEntity);
  }
  m_activeEntity = NULL_ENTITY_ID;
}

void WorldEditorImpl::setCursorDistance(float metres)
{
  m_activeTranslation = metresToWorldUnits(metres);
}

void WorldEditorImpl::setCursorRotation(const Vec3f& ori)
{
  m_activeRotation = ori;
}

void WorldEditorImpl::setCursorScale(float scale)
{
  m_activeScale = Vec3f{ scale, scale, scale } * WORLD_UNITS_PER_METRE;
}

void WorldEditorImpl::constructCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  Vec3f postScaleSize{ 1.f, 1.f, 1.f };
  Vec3f preScaleSize = postScaleSize / m_activeScale;

  m_cursorId = m_engine->ecs().idGen().getNewEntityId();
  m_engine->ecs().componentStore().allocate<DSpatial, DModel>(m_cursorId);

  DSpatial spatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
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

  Vec3f initialPos = metresToWorldUnits(Vec3f{ 11305.0, 30.0, 9199.0 });
  m_engine->ecs().system<SysRender3d>().camera().setPosition(initialPos);

  auto cell = cellFromPosition(initialPos);
  for (int i = 0; i < 6; ++i) {
    auto slice = m_engine->worldLoader().loadCellSliceAsync(cell[0], cell[1], i).wait();
    m_state.slices[{ cell[0], cell[1], i }] = {
      .handle = slice,
      .entities = m_engine->worldLoader().createEntities(slice.id()),
      .dirty = false
    };
  }
}

void WorldEditorImpl::positionCursor()
{
  auto& sysSpatial = m_engine->ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine->ecs().system<SysRender3d>();

  auto& camera = sysRender3d.camera();
  auto camPos = camera.getPosition();
  auto camDir = camera.getDirection();

  auto transform = createTransform(camPos + camDir * m_activeTranslation, m_activeRotation,
    m_activeScale);

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
