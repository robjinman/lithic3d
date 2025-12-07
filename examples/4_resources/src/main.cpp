#include <lithic3d/lithic3d.hpp>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

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
    Scheduler m_scheduler;
    EntityId m_cube = NULL_ENTITY_ID;
    EntityId m_quad = NULL_ENTITY_ID;
    ResourceHandle m_cubeModel;
    bool m_loadingCube = false;

    void loadCubeResources();
    void unloadCubeResources();
    bool cubeResourcesReady() const;
    EntityId constructLight();
    EntityId constructQuad();
    EntityId constructCube();
    EntityId constructCaption();
    void rotateQuad();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  constructLight();
  m_quad = constructQuad();
  constructCaption();

  loadCubeResources();
}

void Demo::loadCubeResources()
{
  auto mesh = render::cuboid({ 1.f, 1.f, 1.f }, { 1.f, 1.f });

  auto model = std::make_unique<Model>();
  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_engine.renderResourceLoader().loadMeshAsync(std::move(mesh)),
      .material = m_factory->createMaterial("textures/bricks.png"),
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  m_cubeModel = m_engine.modelLoader().loadModelAsync(std::move(model));

  m_loadingCube = true;
}

void Demo::unloadCubeResources()
{
  m_scheduler.run(3, [this]() {
    m_cubeModel = ResourceHandle{};
  });
}

bool Demo::cubeResourcesReady() const
{
  return m_cubeModel.ready();
}

EntityId Demo::constructQuad()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DQuad>(id);

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender2d = m_engine.ecs().system<SysRender2d>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  DQuad render;
  render.colour = { 1.f, 0.f, 0.f, 1.f };

  sysRender2d.addEntity(id, render);

  return id;
}

EntityId Demo::constructCube()
{
  // TODO: This is ridiculous
  static bool done = [this]() {
    auto& model = m_engine.modelLoader().getModel(m_cubeModel.id());
    m_engine.renderer().compileShader(false, model.submodels[0]->mesh.features,
      model.submodels[0]->material.features);
    return true;
  }();

  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();
  spatial.aabb = {
    .min = { -0.5f, -0.5f, -0.5f },
    .max = { 0.5f, 0.5f, 0.5f }
  };

  sysSpatial.addEntity(id, spatial);

  auto render = std::make_unique<DModel>();
  render->model = m_cubeModel;

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DLight>(id);

  DSpatial spatial{
    .transform = translationMatrix4x4(Vec3f{ 5.f, 5.f, -2.f }),
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

EntityId Demo::constructCaption()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DText>(id);

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
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
  material->texture = m_engine.renderResourceLoader().loadTextureAsync("textures/fonts.png");

  DText render{
    .scissor = 0,
    .material = m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)),
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

void Demo::rotateCube()
{
  if (m_cube == NULL_ENTITY_ID) {
    return;
  }

  float a = (2 * PIf / 360.f) * (m_engine.currentTick() % 360);
  float b = (2 * PIf / 720.f) * (m_engine.currentTick() % 720);

  m_engine.ecs().system<SysSpatial>().setEntityTransform(m_cube, createTransform({ 0.f, 0.f, 5.f },
    { b, a, 0.f }));
}

void Demo::rotateQuad()
{
  float a = (2 * PIf / 360.f) * (m_engine.currentTick() % 360);

  Vec2f pos{ 0.1f, 0.8f };
  Vec2f size{ 0.08f, 0.08f };
  auto m = screenSpaceTransform(pos, size, a, { 0.5f, 0.5f });
  m_engine.ecs().system<SysSpatial>().setEntityTransform(m_quad, m);
}

void Demo::onKeyDown(KeyboardKey key)
{
  if (key == KeyboardKey::Space) {
    if (m_cube == NULL_ENTITY_ID) {
      loadCubeResources();
    }
    else {
      m_engine.ecs().removeEntity(m_cube);
      unloadCubeResources();
      m_cube = NULL_ENTITY_ID;
    }
  }
}

bool Demo::update()
{
  rotateQuad();
  rotateCube();
  m_engine.update({});
  m_scheduler.update();

  if (m_loadingCube) {
    if (cubeResourcesReady()) {
      m_cube = constructCube();
      m_loadingCube = false;
    }
  }

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Resources",
    .appShortName = "resources",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080,
    .captureMouse = false
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
