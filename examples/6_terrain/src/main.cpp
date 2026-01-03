#include <lithic3d/lithic3d.hpp>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

const float MOUSE_LOOK_SPEED = 3.5f;
const float MOVEMENT_SPEED = metresToWorldUnits(60.f);

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey) override;
    void onKeyUp(KeyboardKey) override;
    void onButtonDown(GamepadButton) override {}
    void onButtonUp(GamepadButton) override {}
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f&, const Vec2f&) override;
    void onLeftStickMove(const Vec2f&) override;
    void onRightStickMove(const Vec2f&) override;
    void onWindowResize(uint32_t, uint32_t) override {}
    void hideMobileControls() override {}
    void showMobileControls() override {}
    bool update() override;

  private:
    Engine& m_engine;
    WorldLoaderPtr m_worldLoader;
    WorldGridPtr m_worldGrid;
    FactoryPtr m_factory;
    EntityFactoryPtr m_entityFactory;
    EntityId m_sun;
    EntityId m_light;
    EntityId m_caption;
    InputState m_inputState;  // TODO: Engine to maintain input state?
    Vec2f m_leftStickDelta;
    Vec2f m_mouseDelta;

    void constructSkybox();
    EntityId constructSun();
    EntityId constructLight();
    EntityId constructCaption();
    void processMouseInput();
    void processKeyboardInput();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_entityFactory = createEntityFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader(), m_engine.resourceManager(), m_engine.fileSystem(),
    m_engine.logger());

  m_worldLoader = createWorldLoader(m_engine.ecs(), m_engine.fileSystem(), *m_entityFactory,
    m_engine.modelLoader(), m_engine.renderResourceLoader(), m_engine.resourceManager(),
    m_engine.logger());

  WorldTraversalOptions options{
    .sliceLoadDistances = { 1, 1, 1, 1, 1, 1 }
  };

  m_worldGrid = createWorldGrid(options, *m_worldLoader, m_engine.ecs(), m_engine.logger());

  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  constructSkybox();
  m_sun = constructSun();
  m_light = constructLight();
  m_caption = constructCaption();

  Vec3f pos{ 25.f, 4.f, 30.f };
  m_engine.ecs().system<SysRender3d>().camera().setPosition(metresToWorldUnits(pos));
}

EntityId Demo::constructSun()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DLight>(id);

  float pitch = degreesToRadians(-45.f);
  float yaw = degreesToRadians(180.f);
  auto m = createTransform(metresToWorldUnits(Vec3f{ 0.f, 10.f, 0.f }), Vec3f{ pitch, yaw, 0 });

  DSpatial spatial{
    .transform = m,
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto& resourceLoader = m_engine.renderResourceLoader();

  Vec4f colour = { 1.f, 1.f, 1.f, 1.f };

  auto lightModel = std::make_unique<Submodel>();
  auto lightMesh = cuboid(metresToWorldUnits(Vec3f{ 1.f, 1.f, 1.f }), { 1.f, 1.f });
  auto lightMaterial = std::make_unique<Material>();
  lightMaterial->colour = colour;
  lightModel->mesh = resourceLoader.loadMeshAsync(std::move(lightMesh));
  lightModel->material = resourceLoader.loadMaterialAsync(std::move(lightMaterial)).wait();

  auto light = std::make_unique<DLight>();
  light->colour = colour.sub<3>();
  light->ambient = 0.5f;
  light->specular = 1.0f;
  light->zFar = metresToWorldUnits(100.f);
  light->submodels.push_back(std::move(lightModel));

  m_engine.ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DLight>(id);

  float pitch = degreesToRadians(-45.f);
  float yaw = degreesToRadians(180.f);
  auto m = createTransform(metresToWorldUnits(Vec3f{ 20.f, 5.f, 30.f }), Vec3f{ pitch, yaw, 0 });

  DSpatial spatial{
    .transform = m,
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto& resourceLoader = m_engine.renderResourceLoader();

  Vec4f colour = { 1.f, 0.2f, 0.f, 1.f };

  auto lightModel = std::make_unique<Submodel>();
  auto lightMesh = cuboid(metresToWorldUnits(Vec3f{ 1.f, 1.f, 1.f }), { 1.f, 1.f });
  auto lightMaterial = std::make_unique<Material>();
  lightMaterial->colour = colour;
  lightModel->mesh = resourceLoader.loadMeshAsync(std::move(lightMesh));
  lightModel->material = resourceLoader.loadMaterialAsync(std::move(lightMaterial)).wait();

  auto light = std::make_unique<DLight>();
  light->colour = colour.sub<3>();
  light->ambient = 0.3f;
  light->specular = 0.4f;
  light->zFar = metresToWorldUnits(100.f);
  light->submodels.push_back(std::move(lightModel));

  m_engine.ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

void Demo::constructSkybox()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DSkybox>(id);

  auto mesh = render::cuboid({ 9999.f, 9999.f, 9999.f }, { 1.f, 1.f });
  mesh->attributeBuffers.resize(1); // Just keep the positions
  mesh->featureSet.vertexLayout = { BufferUsage::AttrPosition };
  mesh->featureSet.flags.set(MeshFeatures::IsSkybox, true);
  auto indices = mesh->indexBuffer.data.data<uint16_t>();
  std::reverse(indices.begin(), indices.end());

  auto material = std::make_unique<Material>();
  material->featureSet.flags.set(MaterialFeatures::HasCubeMap, true);
  material->cubeMap = m_engine.renderResourceLoader().loadCubeMapAsync({
    "textures/skybox/right.png",
    "textures/skybox/left.png",
    "textures/skybox/top.png",
    "textures/skybox/bottom.png",
    "textures/skybox/front.png",
    "textures/skybox/back.png"
  });

  auto render = std::make_unique<DSkybox>();
  render->model = std::make_unique<Submodel>();
  render->model->mesh = m_engine.renderResourceLoader().loadMeshAsync(std::move(mesh));
  render->model->material =
    m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)).wait();

  sysRender3d.addEntity(id, std::move(render));

  DSpatial spatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb{}
  };

  sysSpatial.addEntity(id, spatial);
}

EntityId Demo::constructCaption()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DText>(id);

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto material = std::make_unique<Material>();
  material->featureSet = MaterialFeatureSet{
    .flags{
      bitflag(MaterialFeatures::HasTexture)
    }
  };
  material->textures = {
    m_engine.renderResourceLoader().loadTextureAsync("textures/fonts.png").wait()
  };

  DText render{
    .scissor = 0,
    .material = m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)).wait(),
    .textureRect = {
      .x = pxToUvX(256.f, 1024.f),
      .y = pxToUvY(0.f, 256.f, 256.f),
      .w = pxToUvW(256.f, 1024.f),
      .h = pxToUvH(256.f, 256.f)
    },
    .text = "Welcome to Lithic3D!",
    .zIndex = 0,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  m_engine.ecs().system<SysRender2d>().addEntity(id, render);

  return id;
}

void Demo::processKeyboardInput()
{
  auto& camera = m_engine.ecs().system<SysRender3d>().camera();

  Vec3f direction{};
  float speed = MOVEMENT_SPEED;
  const auto forward = camera.getDirection();
  const auto strafe = forward.cross(Vec3f{0, 1, 0});

  if (m_leftStickDelta != Vec2f{}) {
    float threshold = 0.4f;

    if (m_leftStickDelta.magnitude() > threshold) {
      float x = m_leftStickDelta[0];
      float y = -m_leftStickDelta[1];
      direction = forward * y + strafe * x;
      speed = m_leftStickDelta.magnitude() * MOVEMENT_SPEED;
    }

    m_leftStickDelta = Vec2f{};
  }
  else {
    if (m_inputState.keysPressed.contains(KeyboardKey::W)) {
      direction += forward;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::S)) {
      direction -= forward;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::D)) {
      direction += strafe;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::A)) {
      direction -= strafe;
    }
  }
  
  if (direction != Vec3f{}) {
    direction = direction.normalise();
    auto delta = direction * speed / static_cast<float>(TICKS_PER_SECOND);

    camera.translate(delta);
  }
}

void Demo::processMouseInput()
{
  auto& camera = m_engine.ecs().system<SysRender3d>().camera();

  camera.rotate(-MOUSE_LOOK_SPEED * m_mouseDelta[1], -MOUSE_LOOK_SPEED * m_mouseDelta[0]);
  m_mouseDelta = Vec2f{};
}

bool Demo::update()
{
  auto& camera = m_engine.ecs().system<SysRender3d>().camera();

  processKeyboardInput();
  processMouseInput();
  m_engine.update(m_inputState);
  m_worldGrid->update(camera.getPosition());

  return true;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);

  switch (key) {
    case KeyboardKey::F: {
      m_engine.logger().info(STR("Simulation tick rate: " << m_engine.measuredTickRate()));
      break;
    }
    default: break;
  }
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void Demo::onMouseButtonDown()
{
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void Demo::onMouseButtonUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
  m_mouseDelta = delta;
}

void Demo::onLeftStickMove(const Vec2f& delta)
{

}

void Demo::onRightStickMove(const Vec2f& delta)
{
  const float threshold = 0.1;
  const float sensitivity = 0.01;

  if (delta.squareMagnitude() >= threshold) {
    m_inputState.mouseX += delta[0] * sensitivity;
    m_inputState.mouseY += delta[1] * -sensitivity;
  }
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Terrain",
    .appShortName = "terrain",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080,
    .captureMouse = true,
    .shaderManifest = "shaders.xml"
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
