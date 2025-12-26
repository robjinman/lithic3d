#include <lithic3d/lithic3d.hpp>
#include "player.hpp"

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

const float GRAVITY_STRENGTH = 3.5f;
const float MOUSE_LOOK_SPEED = 3.5f;
const float GRAVITATIONAL_CONSTANT =
  GRAVITY_STRENGTH * metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND);

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onButtonDown(GamepadButton button) override;
    void onButtonUp(GamepadButton button) override;
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f& pos, const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f& delta) override;
    void onRightStickMove(const Vec2f& delta) override;
    void onWindowResize(uint32_t, uint32_t) override {}
    void hideMobileControls() override {}
    void showMobileControls() override {}
    bool update() override;

  private:
    Engine& m_engine;
    FactoryPtr m_factory;
    DModelPtr m_model;
    PlayerPtr m_player;
    InputState m_inputState;  // TODO: Engine to maintain input state?
    Vec2f m_leftStickDelta;
    Vec2f m_mouseDelta;
    float m_playerVerticalVelocity = 0.f;
    bool m_freeflyMode = false;

    DModelPtr loadModel();
    void constructSkybox();
    EntityId constructLight();
    EntityId constructModel();
    EntityId constructGround();
    void processMouseInput();
    void processKeyboardInput();
    void gravity();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  m_player = createPlayer(m_engine.ecs().system<SysRender3d>().camera());
  m_model = loadModel();

  constructSkybox();
  constructLight();
  constructGround();
  constructModel();

  m_engine.renderer().start(); // TODO: Move to engine?
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
  uint16_t* indexData = reinterpret_cast<uint16_t*>(mesh->indexBuffer.data.data());
  std::reverse(indexData, indexData + mesh->indexBuffer.data.size() / sizeof(uint16_t));

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
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DLight>(id);

  float pitch = degreesToRadians(-45.f);
  float yaw = degreesToRadians(180.f);
  auto m = createTransform(metresToWorldUnits(Vec3f{ 0.f, 10.f, -100.f }), Vec3f{ pitch, yaw, 0 });

  DSpatial spatial{
    .transform = m,
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DLight>();
  light->colour = { 1.f, 0.9f, 0.9f };
  light->ambient = 0.4f;
  light->specular = 0.9f;

  m_engine.ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

DModelPtr Demo::loadModel()
{
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto render = std::make_unique<DModel>();
  render->model = m_engine.modelLoader().loadModelAsync("models/hut.gltf").wait();

  return render;
}

EntityId Demo::constructModel()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  float x = 0.f;
  float z = 0.f;

  auto m = translationMatrix4x4(metresToWorldUnits(Vec3f{ x, 0.f, z }))
    * scaleMatrix4x4(Vec3f{ 10.f, 10.f, 10.f });

  DSpatial spatial{
    .transform = m,
    .parent = sysSpatial.root(),
    .enabled = true,
    .aabb = {
      .min = { -1.f, -1.f, -1.f },  // TODO: Compute from mesh
      .max = { 1.f, 1.f, 1.f }
    }
  };

  sysSpatial.addEntity(id, spatial);

  DModelPtr render = std::make_unique<DModel>();
  render->model = m_model->model;

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

EntityId Demo::constructGround()
{
  auto material = m_factory->createMaterial("textures/ground.png").wait();

  auto id = m_factory->createCuboid(metresToWorldUnits(Vec3f{ 50.f, 1.f, 50.f }), material,
    metresToWorldUnits(Vec2f{ 5.f, 5.f }));

  m_engine.ecs().componentStore().component<CLocalTransform>(id).transform =
    translationMatrix4x4(Vec3f{ 0.f, -metresToWorldUnits(0.5f), 0.f });

  return id;
}

void Demo::processKeyboardInput()
{
  Vec3f direction{};
  float speed = m_player->getSpeed();
  const auto forward = m_player->getDirection();
  const auto strafe = m_player->getDirection().cross(Vec3f{0, 1, 0});

  if (m_leftStickDelta != Vec2f{}) {
    float threshold = 0.4f;

    if (m_leftStickDelta.magnitude() > threshold) {
      float x = m_leftStickDelta[0];
      float y = -m_leftStickDelta[1];
      direction = forward * y + strafe * x;
      speed = m_leftStickDelta.magnitude() * m_player->getSpeed();
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

  if (m_inputState.keysPressed.contains(KeyboardKey::E)) {
    if (m_player->getPosition()[1] == 0) {
      m_playerVerticalVelocity = sqrt(m_player->getJumpHeight() * 2.f * GRAVITATIONAL_CONSTANT);
    }
  }
  
  if (direction != Vec3f{}) {
    if (!m_freeflyMode) {
      direction[1] = 0;
    }

    direction = direction.normalise();
    auto playerPos = m_player->getPosition();
    auto delta = direction * speed / static_cast<float>(TICKS_PER_SECOND);

    m_player->translate(delta);
  }
}

void Demo::processMouseInput()
{
  m_player->rotate(-MOUSE_LOOK_SPEED * m_mouseDelta[1], -MOUSE_LOOK_SPEED * m_mouseDelta[0]);
  m_mouseDelta = Vec2f{};
}

void Demo::gravity()
{
  m_player->translate(Vec3f{ 0, m_playerVerticalVelocity, 0 });
  float altitude = m_player->getPosition()[1];

  if (altitude > 0) {
    m_playerVerticalVelocity =
      std::max(m_playerVerticalVelocity - GRAVITATIONAL_CONSTANT, -altitude);
  }
  else if (altitude == 0) {
    m_playerVerticalVelocity = 0;
  }
}

bool Demo::update()
{
  processKeyboardInput();
  processMouseInput();
  gravity();
  m_engine.update(m_inputState);

  return true;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);

  if (key == KeyboardKey::F) {
    m_engine.logger().info(STR("Simulation tick rate: " << m_engine.measuredTickRate()));
  }
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void Demo::onButtonDown(GamepadButton button)
{

}

void Demo::onButtonUp(GamepadButton button)
{

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
    .appDisplayName = "Lithic3D Demo - Scene",
    .appShortName = "scene",
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
