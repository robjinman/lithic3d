#include <lithic3d/lithic3d.hpp>
#include <random>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

float VIEW_X = 280.f;
float VIEW_Y = 6.f;
float VIEW_Z = 300.f;

struct Object
{
  bool randomRotation;
  Vec3f dimensions;
  Vec3f position;
  Vec3f rotation;
  bool infiniteMass;
};

struct Scenario
{
  std::vector<Object> objects;
};

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey) override;
    void onKeyUp(KeyboardKey) override {}
    void onButtonDown(GamepadButton) override {}
    void onButtonUp(GamepadButton) override {}
    void onMouseButtonDown() override {}
    void onMouseButtonUp() override {}
    void onMouseMove(const Vec2f&, const Vec2f&) override {}
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
    std::vector<Scenario> m_scenarios = {
      Scenario{
        .objects = {
          Object{
            .randomRotation = false,
            .dimensions = { 2.f, 2.f, 1.f },
            .position = { VIEW_X + 2.6f, VIEW_Y + 2.f, VIEW_Z - 15.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(45.f) },
            .infiniteMass = false
          },
          Object{
            .randomRotation = false,
            .dimensions = { 5.f, 0.5f, 5.f },
            .position = { VIEW_X + 0.f, VIEW_Y - 4.f, VIEW_Z - 15.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
            .infiniteMass = true
          }
        }
      },
      Scenario{
        .objects = {
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X - 0.f, VIEW_Y + 1.f, VIEW_Z - 15.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
            .infiniteMass = false
          },
          Object{
            .randomRotation = false,
            .dimensions = { 4.f, 1.f, 4.f },
            .position = { VIEW_X + 0.f, VIEW_Y - 3.f, VIEW_Z - 15.f },
            .rotation = { degreesToRadians(15.f), degreesToRadians(-7.f), degreesToRadians(6.f) },
            .infiniteMass = false
          }
        }
      },
      Scenario{
        .objects = {
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 3.f, 0.5f },
            .position = { VIEW_X - 0.f, VIEW_Y + 3.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 2.f, 1.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 0.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          }
        }
      },
      Scenario{
        .objects = {
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 3.f, 0.5f },
            .position = { VIEW_X - 0.f, VIEW_Y + 1.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 2.f, 1.5f },
            .position = { VIEW_X + 0.f, VIEW_Y- 2.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X + 0.f, VIEW_Y + 3.5f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 13.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 14.f },
            .rotation = {},
            .infiniteMass = false
          },
          Object{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X - 1.f, VIEW_Y + 5.5f, VIEW_Z - 14.f },
            .rotation = {},
            .infiniteMass = false
          },
        }
      }
    };
    size_t m_currentScenario = 3;
    std::vector<EntityId> m_entityIds;
    bool m_physicsActive = false;

    EntityId constructLight();
    void constructScenario(size_t i);
    void destroyScenario();
    void constructGround();
    EntityId constructCaption();
    void resetState();
    void enablePhysics();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  m_light = constructLight();
  constructGround();
  m_caption = constructCaption();

  auto& camera = m_engine.ecs().system<SysRender3d>().camera();
  camera.setPosition(metresToWorldUnits(Vec3f{ VIEW_X, VIEW_Y, VIEW_Z }));
  camera.rotate(-degreesToRadians(15.f), 0.f);

  constructScenario(m_currentScenario);

  // TODO: Delete
  for (size_t i = 0; i < 6; ++i) {
    resetState();
  }

  enablePhysics();

  for (size_t i = 0; i < 476; ++i) {
    //onKeyDown(KeyboardKey::N);
  }
}

Vec3f randomRotation()
{
  static std::mt19937 gen;
  std::uniform_real_distribution<float> dist(0, 2.f * PIf);

  return { dist(gen), dist(gen), dist(gen) };
}

void Demo::constructScenario(size_t i)
{
  assert(m_entityIds.empty());

  for (auto& obj : m_scenarios[i].objects) {
    auto material = m_factory->createMaterialAsync("textures/bricks.png");
    auto size = metresToWorldUnits(obj.dimensions);
    auto texSize = metresToWorldUnits(Vec2f{ 1.f, 1.f });
    auto id = m_factory->createDynamicCuboid(size, material, texSize, 0.f, 0.2f, 0.4f);
    m_entityIds.push_back(id);
  }

  resetState();
}

void Demo::destroyScenario()
{
  for (auto id : m_entityIds) {
    m_engine.eventSystem().raiseEvent(ERequestDeletion{id});
  }
  m_entityIds.clear();
}

void Demo::constructGround()
{
  TerrainConfig terrainConfig{
    .world = "world",
    .minHeight = 0.f,
    .maxHeight = 6.f,
    .cellWidth = 400.f,
    .cellHeight = 400.f
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

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  Vec3f pos = metresToWorldUnits(Vec3f{ VIEW_X + 2.f, VIEW_Y + 7.f, VIEW_Z + 5.f });

  DSpatial spatial{
    .transform = createTransform(pos, { -degreesToRadians(45.f), 0.f, 0.f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 1.f, 1.f };
  light->ambient = 0.1f;
  light->specular = 0.2f;

  m_engine.ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

EntityId Demo::constructCaption()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DText>(id);

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.2f, 0.85f }, { 0.025f, 0.05f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
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
      .x = pxToUvX(0.f, 1024.f),
      .y = pxToUvY(0.f, 256.f, 256.f),
      .w = pxToUvW(256.f, 1024.f),
      .h = pxToUvH(256.f, 256.f)
    },
    .text = "Press space to drop, and again to reset",
    .zIndex = 0,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  m_engine.ecs().system<SysRender2d>().addEntity(id, render);

  return id;
}

void Demo::enablePhysics()
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();

  assert(m_entityIds.size() == m_scenarios[m_currentScenario].objects.size());

  for (size_t i = 0; i < m_entityIds.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].objects[i];
    auto id = m_entityIds[i];

    if (!obj.infiniteMass) {
      auto invMass = 1.f /
        (obj.dimensions[0] * obj.dimensions[1] * obj.dimensions[2] * WORLD_UNITS_PER_METRE);
      sysCollision.setInverseMass(id, invMass);
    }
  }

  m_physicsActive = true;
}

void Demo::resetState()
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

  assert(m_entityIds.size() == m_scenarios[m_currentScenario].objects.size());

  for (size_t i = 0; i < m_entityIds.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].objects[i];
    auto id = m_entityIds[i];

    auto rotation = obj.randomRotation ? randomRotation() : obj.rotation;
    auto transform = createTransform(metresToWorldUnits(obj.position), rotation);

    sysCollision.setInverseMass(id, 0.f);
    sysCollision.setStationary(id);
    sysSpatial.setEntityTransform(id, transform);
  }

  m_physicsActive = false;
}

void Demo::onKeyDown(KeyboardKey key)
{
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
  else if (key == KeyboardKey::Left) {
    if (m_currentScenario > 0) {
      destroyScenario();
      constructScenario(--m_currentScenario);
    }
  }
  else if (key == KeyboardKey::Right) {
    if (m_currentScenario + 1 < m_scenarios.size()) {
      destroyScenario();
      constructScenario(++m_currentScenario);
    }
  }
  else if (key == KeyboardKey::N) {
    static Tick tick = 0;
    m_engine.logger().info(STR("Tick: " << tick));
    m_engine.ecs().system<SysCollision>().update(tick++, {});
  }
}

bool Demo::update()
{
  m_engine.update({});

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Physics",
    .appShortName = "physics",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080,
    .captureMouse = false,
    .shaderManifest = "shaders.xml"
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
