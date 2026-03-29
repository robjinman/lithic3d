#include "player.hpp"
#include <lithic3d/lithic3d.hpp>
#include <random>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

const float MOUSE_LOOK_SPEED = 3.5f;

struct Box
{
  Vec3f size;
  Vec3f position;
  Vec3f rotation;
  bool randomRotation;
};

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onButtonDown(GamepadButton) override {}
    void onButtonUp(GamepadButton) override {}
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f& pos, const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f&) override {}
    void onRightStickMove(const Vec2f&) override {}
    void onWindowResize(uint32_t, uint32_t) override {}
    void hideMobileControls() override {}
    void showMobileControls() override {}
    bool update() override;

  private:
    Engine& m_engine;
    FactoryPtr m_factory;
    EntityId m_light;
    EntityId m_caption;
    ResourceHandle m_terrain;
    PlayerPtr m_player;
    InputState m_inputState;
    Vec2f m_mouseDelta;
    std::vector<EntityId> m_dynamicBoxIds;
    bool m_physicsActive = false;
    std::vector<Box> m_dynamicBoxes = {
      Box{
        .size = { 1.f, 1.f, 1.f },
        .position = { 0.f, 0.f, 2.f },
        .rotation = {},
        .randomRotation = true
      }
    };

    void constructLight();
    void constructDynamicBoxes();
    void constructGround();
    void constructTerrain();
    void constructCaption();
    void resetState();
    void enablePhysics();
    void processKeyboardInput();
    void processMouseInput();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  m_player = createPlayer(m_engine.ecs(), m_engine.renderResourceLoader(), m_engine.modelLoader());
  constructLight();
  //constructGround();
  constructTerrain();
  constructCaption();
  //constructDynamicBoxes();

  m_player->setPosition(metresToWorldUnits(Vec3f{ 50.f, 5.f, 50.f }));

  //resetState();
}

Vec3f randomRotation()
{
  static std::mt19937 gen;
  std::uniform_real_distribution<float> dist(0, 2.f * PIf);

  return { dist(gen), dist(gen), dist(gen) };
}

void Demo::constructDynamicBoxes()
{
  auto material = m_factory->createMaterialAsync("textures/bricks.png");
  auto texSize = metresToWorldUnits(Vec2f{ 1.f, 1.f });

  for (auto& box : m_dynamicBoxes) {
    auto size = metresToWorldUnits(box.size);
    EntityId id = m_factory->createDynamicCuboid(size, material, texSize, 0.f, 0.2f, 0.4f);
    m_dynamicBoxIds.push_back(id);
  }
}

void Demo::constructTerrain()
{
  TerrainConfig terrainConfig{
    .world = "world",
    .minHeight = 0.f,
    .maxHeight = 8.f,
    .cellWidth = 200.f,
    .cellHeight = 200.f
  };

  auto terrainBuilder = createTerrainBuilder(terrainConfig, m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader(), m_engine.resourceManager(), m_engine.fileSystem(),
    m_engine.logger());

  const char* xmlTerrain =
    "<terrain>"
      "<splat-map>"
        "<texture file=\"sand.png\"/>"
        "<texture file=\"grass.png\"/>"
        "<texture file=\"dirt.png\"/>"
        "<texture file=\"snow.png\"/>"
      "</splat-map>"
    "</terrain>";

  m_terrain = terrainBuilder->loadTerrainRegionAsync(0, 0, parseXml(xmlTerrain)).wait();
  terrainBuilder->createEntities(m_terrain.id());
}

void Demo::constructGround()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto material = m_factory->createMaterialAsync("textures/grass.png");
  auto size = metresToWorldUnits(Vec3f{ 200.f, 4.f, 200.f });
  auto texSize = metresToWorldUnits(Vec2f{ 10.f, 10.f });
  auto id = m_factory->createDynamicCuboid(size, material, texSize, 0.f, 0.2f, 0.4f); // TODO: Use static
  sysSpatial.setLocalTransform(id, translationMatrix4x4(size * 0.5f));
}

void Demo::constructLight()
{
  m_light = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DDirectionalLight>(m_light);

  Vec3f pos = metresToWorldUnits(Vec3f{ -50.f, 50.f, -50.f });

  DSpatial spatial{
    .transform = createTransform(pos, { -degreesToRadians(45.f), 0.f, 0.f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(m_light, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 1.f, 1.f };
  light->ambient = 0.1f;
  light->specular = 0.2f;

  m_engine.ecs().system<SysRender3d>().addEntity(m_light, std::move(light));
}

void Demo::constructCaption()
{
  m_caption = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DText>(m_caption);

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.2f, 0.85f }, { 0.025f, 0.05f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(m_caption, spatial);

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
      .x = pxToUvX(0.f, 1024.f),
      .y = pxToUvY(0.f, 256.f, 256.f),
      .w = pxToUvW(256.f, 1024.f),
      .h = pxToUvH(256.f, 256.f)
    },
    .text = "Press space to drop, and again to reset",
    .zIndex = 0,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  m_engine.ecs().system<SysRender2d>().addEntity(m_caption, render);
}

void Demo::enablePhysics()
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();

  assert(m_dynamicBoxIds.size() == m_dynamicBoxes.size());

  for (size_t i = 0; i < m_dynamicBoxIds.size(); ++i) {
    auto& obj = m_dynamicBoxes[i];
    auto id = m_dynamicBoxIds[i];

    auto invMass = 1.f / (obj.size[0] * obj.size[1] * obj.size[2]);
    sysCollision.setInverseMass(id, invMass);
  }

  m_physicsActive = true;
}

void Demo::resetState()
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

  assert(m_dynamicBoxIds.size() == m_dynamicBoxes.size());

  for (size_t i = 0; i < m_dynamicBoxIds.size(); ++i) {
    auto& obj = m_dynamicBoxes[i];
    auto id = m_dynamicBoxIds[i];

    sysCollision.setInverseMass(id, 0.f);
    sysCollision.setStationary(id);

    auto rotation = obj.randomRotation ? randomRotation() : obj.rotation;
    auto transform = createTransform(metresToWorldUnits(obj.position), rotation);

    sysSpatial.setLocalTransform(id, transform);
  }

  m_physicsActive = false;
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
  m_mouseDelta = delta;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);

  if (key == KeyboardKey::Space) {
    if (m_physicsActive) {
      resetState();
    }
    else {
      enablePhysics();
    }
  }
  else if (key == KeyboardKey::F) {
    m_engine.logger().info(STR("Simulation tick rate: " << m_engine.measuredTickRate()));
  }
  else if (key == KeyboardKey::N) {
    static Tick tick = 0;
    m_engine.logger().info(STR("Tick: " << tick));
    m_engine.ecs().system<SysCollision>().update(tick++, {});
  }
  else if (key == KeyboardKey::E) {
    m_engine.logger().info("Jump!");
    m_engine.ecs().system<SysCollision>().applyForce(m_player->getId(), { 0.f, 1.f, 0.f }, 0.3f);
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

void Demo::processKeyboardInput()
{
  float metresPerSecond = 2.f;
  float speed = metresToWorldUnits(metresPerSecond) / TICKS_PER_SECOND;
  const auto forward = m_player->getDirection();
  const auto right = m_player->getDirection().cross(Vec3f{0, 1, 0});
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

  if (direction != Vec3f{}) {
    direction[1] = 0;

    direction = direction.normalise();
    auto playerPos = m_player->getPosition();
    auto delta = direction * speed;

    m_player->translate(delta);
  }
}

void Demo::processMouseInput()
{
  m_player->rotate(-MOUSE_LOOK_SPEED * m_mouseDelta[1], MOUSE_LOOK_SPEED * m_mouseDelta[0]);
  m_mouseDelta = Vec2f{};
}

bool Demo::update()
{
  m_engine.update(m_inputState);

  processKeyboardInput();
  processMouseInput();
  m_player->update();

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Zoheim",
    .appShortName = "zoheim",
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
