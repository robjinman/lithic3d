#include <lithic3d/lithic3d.hpp>
#include <random>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

float VIEW_X = 90.f;
float VIEW_Y = 14.f;
float VIEW_Z = 175.f;

struct Box
{
  bool randomRotation;
  Vec3f dimensions;
  Vec3f position;
  Vec3f rotation;
  bool infiniteMass;
  bool isStatic;
};

struct Capsule
{
  Vec3f position;
  float height;
  float radius;
  float inverseMass;
  bool isControllable = false;
};

struct AggregateBox
{
  Vec3f dimensions;
  Vec3f position;
  Vec3f rotation;
};

struct Aggregate
{
  Vec3f position;
  Vec3f rotation;
  std::vector<AggregateBox> boxes;
};

// TODO: Refactor into static/dynamic sub-structs?
struct Scenario
{
  std::vector<Box> boxes;
  std::vector<Capsule> capsules;
  std::vector<Aggregate> aggregates;
};

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey) override;
    void onKeyUp(KeyboardKey) override;
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
        .boxes = {
          Box{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 0.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 1.f, VIEW_Y + 0.f, VIEW_Z - 15.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          }
        },
        .capsules{},
        .aggregates = {
          Aggregate{
            .position = { VIEW_X + 0.f, VIEW_Y - 6.f, VIEW_Z - 15.f },
            .rotation = {},
            .boxes{
              AggregateBox{
                .dimensions = { 1.f, 1.f, 1.f },
                .position = { 0.f, 0.f, 0.f },
                .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
              },
              AggregateBox{
                .dimensions = { 1.f, 1.f, 1.f },
                .position = { 1.f, 0.f, 0.f },
                .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
              }
            }
          }
        }
      },
      Scenario{
        .boxes = {
          Box{
            .randomRotation = true,
            .dimensions = { 2.f, 1.f, 2.f },
            .position = { VIEW_X + 0.f, VIEW_Y + 2.f, VIEW_Z - 25.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(45.f), degreesToRadians(180.f) },
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = false,
            .dimensions = { 8.f, 0.5f, 4.f },
            .position = { VIEW_X + 0.f, VIEW_Y - 7.f, VIEW_Z - 25.f },
            .rotation = { degreesToRadians(30.f), degreesToRadians(0.f), degreesToRadians(0.f) },
            .infiniteMass = false,
            .isStatic = true
          }
        },
        .capsules{},
        .aggregates{}
      },
      Scenario{
        .boxes = {
          Box{
            .randomRotation = false,
            .dimensions = { 8.f, 0.5f, 4.f },
            .position = { VIEW_X + 0.f, VIEW_Y - 7.f, VIEW_Z - 25.f },
            .rotation = { degreesToRadians(39.f), degreesToRadians(12.f), degreesToRadians(6.f) },
            .infiniteMass = false,
            .isStatic = true
          }
        },
        .capsules{
          Capsule{
            .position = { VIEW_X + 0.f, VIEW_Y + 1.f, VIEW_Z - 25.f },
            .height = 2.f,
            .radius = 0.6f,
            .inverseMass = 0.1f,
            .isControllable = true
          }
        },
        .aggregates{}
      },
      Scenario{
        .boxes = {
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X - 0.f, VIEW_Y + 1.f, VIEW_Z - 20.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = false,
            .dimensions = { 4.f, 1.f, 4.f },
            .position = { VIEW_X + 0.f, VIEW_Y - 2.f, VIEW_Z - 20.f },
            .rotation = { degreesToRadians(0.f), degreesToRadians(0.f), degreesToRadians(0.f) },
            .infiniteMass = false,
            .isStatic = false
          }
        },
        .capsules{},
        .aggregates{}
      },
      Scenario{
        .boxes = {
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 3.f, 0.5f },
            .position = { VIEW_X - 0.f, VIEW_Y + 3.f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 2.f, 1.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 0.f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          }
        },
        .capsules{},
        .aggregates{}
      },
      Scenario{
        .boxes = {
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 3.f, 0.5f },
            .position = { VIEW_X - 0.f, VIEW_Y + 1.f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 2.f, 1.5f },
            .position = { VIEW_X + 0.f, VIEW_Y- 2.f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 1.f, 1.f, 1.f },
            .position = { VIEW_X + 0.f, VIEW_Y + 3.5f, VIEW_Z - 20.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 18.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X + 0.f, VIEW_Y + 5.5f, VIEW_Z - 19.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
          Box{
            .randomRotation = true,
            .dimensions = { 0.5f, 0.5f, 0.5f },
            .position = { VIEW_X - 1.f, VIEW_Y + 5.5f, VIEW_Z - 19.f },
            .rotation = {},
            .infiniteMass = false,
            .isStatic = false
          },
        },
        .capsules{},
        .aggregates{}
      }
    };
    size_t m_currentScenario = 1;
    std::vector<EntityId> m_boxes;    //
    std::vector<EntityId> m_capsules; // TODO: Replace with single m_dynamicEntities vector?
    std::vector<EntityId> m_aggregates;
    bool m_physicsActive = false;
    EntityId m_controllableEntity = NULL_ENTITY_ID;
    InputState m_inputState;

    EntityId constructLight();
    void constructScenario(size_t i);
    void destroyScenario();
    void constructGround();
    void constructTerrain();
    EntityId constructCaption();
    void resetState();
    void enablePhysics();
    void processKeyboardInput();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  m_light = constructLight();
  //constructGround();
  constructTerrain();
  m_caption = constructCaption();

  auto& camera = m_engine.ecs().system<SysRender3d>().camera();
  camera.setPosition(metresToWorldUnits(Vec3f{ VIEW_X, VIEW_Y, VIEW_Z }));
  camera.rotate(-degreesToRadians(20.f), 0.f);

  constructScenario(m_currentScenario);

  // TODO: Delete
  //for (size_t i = 0; i < 6; ++i) {
  //  resetState();
  //}

  //enablePhysics();

  //for (size_t i = 0; i < 115; ++i) {
  //  onKeyDown(KeyboardKey::N);
  //}
}

Vec3f randomRotation()
{
  static std::mt19937 gen;
  std::uniform_real_distribution<float> dist(0, 2.f * PIf);

  return { dist(gen), dist(gen), dist(gen) };
}

void Demo::constructScenario(size_t i)
{
  assert(m_boxes.empty());
  assert(m_capsules.empty());
  assert(m_aggregates.empty());

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();
  auto& sysCollision = m_engine.ecs().system<SysCollision>();

  auto material = m_factory->createMaterialAsync("bricks.png", true);
  auto texSize = Vec2f{ 1.f, 1.f };

  for (auto& obj : m_scenarios[i].boxes) {
    auto size = obj.dimensions;
    EntityId id = 0;
    if (obj.isStatic) {
      id = m_factory->createStaticCuboid(sysSpatial.root(), size, material, texSize, 0.2f, 0.4f);
    }
    else {
      id = m_factory->createDynamicCuboid(sysSpatial.root(), size, material, texSize, 0.f, 0.2f,
        0.5f);
    }
    m_boxes.push_back(id);
  }

  for (auto& obj : m_scenarios[i].capsules) {
    auto id = m_engine.ecs().idGen().getNewEntityId();
    m_engine.ecs().componentStore().allocate<DSpatial, DModel, DCapsule>(id);

    DSpatial spatial{};
    spatial.parent = sysSpatial.root();
    spatial.aabb = {
      .min = metresToWorldUnits(Vec3f{ -obj.radius, -obj.height * 0.5f, -obj.radius }),
      .max = metresToWorldUnits(Vec3f{ obj.radius, obj.height * 0.5f, obj.radius })
    };

    sysSpatial.addEntity(id, spatial);

    auto mesh = render::capsule(obj.height, obj.radius);
    mesh->featureSet.flags.set(MeshFeatures::CastsShadow);

    auto material = std::make_unique<Material>();
    material->colour = { 0.f, 1.f, 0.f, 1.f };

    auto model = std::make_unique<Model>();
    model->submodels.push_back(
      std::unique_ptr<Submodel>(new Submodel{
        .lods = { m_engine.renderResourceLoader().loadMeshAsync(std::move(mesh)).wait() },
        .material = m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)).wait(),
        .skin = nullptr,
        .jointTransforms{}
      })
    );

    auto render = std::make_unique<DModel>();
    render->model = m_engine.modelLoader().loadModelAsync(std::move(model)).wait();

    sysRender3d.addEntity(id, std::move(render));

    DCapsule collision{
      .inverseMass = obj.inverseMass,
      .restitution = 0.f,
      .friction = 0.4f,
      .capsule = {
        .radius = metresToWorldUnits(obj.radius),
        .height = metresToWorldUnits(obj.height),
        .translation{}
      }
    };

    sysCollision.addEntity(id, collision);

    if (obj.isControllable) {
      ASSERT(m_controllableEntity == NULL_ENTITY_ID, "Only 1 capsule can be controllable");
      m_controllableEntity = id;
    }

    m_capsules.push_back(id);
  }

  for (auto& obj : m_scenarios[i].aggregates) {
    auto id = m_engine.ecs().idGen().getNewEntityId();
    m_engine.ecs().componentStore().allocate<DSpatial, DModel, DAggregate>(id);

    DAggregate collision{};
    auto model = std::make_unique<Model>();
    Aabb bounds;

    for (size_t j = 0; j < obj.boxes.size(); ++j) {
      auto& box = obj.boxes[j];

      auto mesh = render::cuboid(box.dimensions, texSize);
      mesh->featureSet = MeshFeatureSet{
        .vertexLayout = {
          BufferUsage::AttrPosition,
          BufferUsage::AttrNormal,
          BufferUsage::AttrTexCoord
        },
        .flags{ bitflag(MeshFeatures::CastsShadow) }
      };
      // Mesh model space is in metres
      mesh->transform = createTransform(box.position, box.rotation);

      model->submodels.push_back(
        std::unique_ptr<Submodel>(new Submodel{
          .lods = { m_engine.renderResourceLoader().loadMeshAsync(std::move(mesh)).wait() },
          .material = material.wait(),
          .skin = nullptr,
          .jointTransforms{}
        })
      );

      auto worldUnitsTransform = createTransform(metresToWorldUnits(box.position), box.rotation);

      auto aabb = transformAabb({
        .min = -metresToWorldUnits(box.dimensions) * 0.5f,
        .max = metresToWorldUnits(box.dimensions) * 0.5f
      }, worldUnitsTransform);

      if (j == 0) {
        bounds = aabb;
      }
      else {
        bounds.add(aabb);
      }

      collision.boxes.push_back(DStaticBox{
        .restitution = 0.2f,
        .friction = 0.4f,
        .boundingBox = {
          .min = -metresToWorldUnits(box.dimensions) * 0.5f,
          .max = metresToWorldUnits(box.dimensions) * 0.5f,
          .transform = identityMatrix<4>()
        }
      });
      collision.boxTransforms.push_back(worldUnitsTransform);
    }

    DSpatial spatial{};
    spatial.parent = sysSpatial.root();
    spatial.aabb = bounds;
    spatial.transform = createTransform(metresToWorldUnits(obj.position), obj.rotation);

    sysSpatial.addEntity(id, spatial);

    auto render = std::make_unique<DModel>();
    render->model = m_engine.modelLoader().loadModelAsync(std::move(model)).wait();

    sysRender3d.addEntity(id, std::move(render));

    sysCollision.addEntity(id, collision);

    m_aggregates.push_back(id);
  }

  resetState();
}

void Demo::destroyScenario()
{
  for (auto id : m_boxes) {
    m_engine.eventSystem().raiseEvent(ERequestDeletion{id});
  }
  m_boxes.clear();

  for (auto id : m_capsules) {
    m_engine.eventSystem().raiseEvent(ERequestDeletion{id});
  }
  m_capsules.clear();

  for (auto id : m_aggregates) {
    m_engine.eventSystem().raiseEvent(ERequestDeletion{id});
  }
  m_aggregates.clear();

  m_controllableEntity = NULL_ENTITY_ID;
}

void Demo::constructTerrain()
{
  TerrainConfig terrainConfig{
    .world = "world",
    .minHeight = 0.f,
    .maxHeight = 8.f,
    .cellWidth = 400.f,
    .cellHeight = 400.f,
    .waterLevel = 0.f
  };

  auto terrainBuilder = createTerrainBuilder(terrainConfig, m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader(), m_engine.resourceManager(), m_engine.dataPaths(),
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
  terrainBuilder->createEntities(m_engine.ecs().system<SysSpatial>().root(), m_terrain.id());
}

void Demo::constructGround()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto material = m_factory->createMaterialAsync("grass.png", true);
  auto size = Vec3f{ 200.f, 4.f, 200.f };
  auto texSize = Vec2f{ 10.f, 10.f };
  auto id = m_factory->createDynamicCuboid(sysSpatial.root(), size, material, texSize, 0.f, 0.2f,
    0.4f); // TODO: Use static
  sysSpatial.setLocalTransform(id, translationMatrix4x4(metresToWorldUnits(size * 0.5f)));
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  Vec3f pos = metresToWorldUnits(Vec3f{ VIEW_X + 2.f, VIEW_Y + 7.f, VIEW_Z + 5.f });

  DSpatial spatial{
    .transform = createTransform(pos, { -degreesToRadians(45.f), 0.f, 0.f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
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
    m_engine.renderResourceLoader().loadTextureAsync("fonts.png", false).wait()
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

  assert(m_boxes.size() == m_scenarios[m_currentScenario].boxes.size());
  assert(m_capsules.size() == m_scenarios[m_currentScenario].capsules.size());
  assert(m_aggregates.size() == m_scenarios[m_currentScenario].aggregates.size());

  for (size_t i = 0; i < m_boxes.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].boxes[i];
    auto id = m_boxes[i];

    if (!obj.infiniteMass && !obj.isStatic) {
      const float scale = 1.f;
      auto invMass = 1.f / (obj.dimensions[0] * obj.dimensions[1] * obj.dimensions[2] * scale);
      sysCollision.setInverseMass(id, invMass);
    }
  }

  for (size_t i = 0; i < m_capsules.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].capsules[i];
    auto id = m_capsules[i];
    sysCollision.setInverseMass(id, obj.inverseMass);
  }

  m_physicsActive = true;
}

void Demo::resetState()
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();

  assert(m_boxes.size() == m_scenarios[m_currentScenario].boxes.size());
  assert(m_capsules.size() == m_scenarios[m_currentScenario].capsules.size());
  assert(m_aggregates.size() == m_scenarios[m_currentScenario].aggregates.size());

  for (size_t i = 0; i < m_boxes.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].boxes[i];
    auto id = m_boxes[i];

    if (!obj.isStatic) {
      sysCollision.setInverseMass(id, 0.f);
      sysCollision.setStationary(id);
    }

    auto rotation = obj.randomRotation ? randomRotation() : obj.rotation;
    auto transform = createTransform(metresToWorldUnits(obj.position), rotation);

    sysSpatial.setLocalTransform(id, transform);
  }

  for (size_t i = 0; i < m_capsules.size(); ++i) {
    auto& obj = m_scenarios[m_currentScenario].capsules[i];
    auto id = m_capsules[i];

    sysCollision.setInverseMass(id, 0.f);
    sysCollision.setStationary(id);

    auto transform = translationMatrix4x4(metresToWorldUnits(obj.position));
    sysSpatial.setLocalTransform(id, transform);
  }

  m_physicsActive = false;
}

void Demo::processKeyboardInput()
{
  if (m_controllableEntity != NULL_ENTITY_ID) {
    float metresPerSecond = 3.f;
    float speed = metresToWorldUnits(metresPerSecond) / TICKS_PER_SECOND;
    Vec3f forward = { 0.f, 0.f, -1.f };
    Vec3f right = { 1.f, 0.f, 0.f };

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
      direction = direction.normalise();
      auto delta = direction * speed;

      auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
      sysSpatial.translateEntityLocal(m_controllableEntity, delta);
    }
  }
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void Demo::onKeyDown(KeyboardKey key)
{
  auto& sysCollision = m_engine.ecs().system<SysCollision>();

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
    sysCollision.update(tick++, {});
  }
  else if (key == KeyboardKey::E) {
    if (m_controllableEntity != NULL_ENTITY_ID) {
      sysCollision.applyForce(m_controllableEntity, { 0.f, 1.f, 0.f }, 0.3f);
    }
  }
}

bool Demo::update()
{
  //if (m_currentScenario == 2) {
  //  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  //  auto t = sysSpatial.getLocalTransform(m_controllableEntity);
  //  m_engine.ecs().system<SysRender3d>().camera().setTransform(translationMatrix4x4({ 0.f, 0.5f * 2.f, 0.f }) * t);
  //}

  m_engine.update({});
  processKeyboardInput();

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
    .aspectRatio = 64.f / 48.f,
    .captureMouse = false,
    .paths{},
    .features{},
    .drawDistance = 1000.f
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
