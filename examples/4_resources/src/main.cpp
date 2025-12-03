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
    EntityId m_light;
    EntityId m_cube;
    EntityId m_caption;
    ResourceHandle m_cubeMaterial;
    ResourceHandle m_cubeMesh;
    MaterialFeatureSet m_materialFeatures;
    MeshFeatureSet m_meshFeatures;
    bool m_loadingCube = false;

    void loadCubeResources();
    void unloadCubeResources();
    bool cubeResourcesReady() const;

    EntityId constructLight();
    EntityId constructCube();
    EntityId constructCaption();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.renderResourceLoader());

  m_light = constructLight();
  m_cube = NULL_ENTITY_ID;
  m_caption = constructCaption();

  loadCubeResources();
}

void Demo::loadCubeResources()
{
  auto material = m_factory->createMaterial("textures/bricks.png");
  m_cubeMaterial = material.resource;
  m_materialFeatures = material.features;

  m_meshFeatures = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  auto mesh = render::cuboid({ 1.f, 1.f, 1.f }, { 1.f, 1.f });
  mesh->featureSet = m_meshFeatures;

  m_cubeMesh = m_engine.renderResourceLoader().loadMeshAsync(std::move(mesh));

  m_loadingCube = true;
}

void Demo::unloadCubeResources()
{
  m_cubeMaterial = ResourceHandle{};
  m_cubeMesh = ResourceHandle{};
}

bool Demo::cubeResourcesReady() const
{
  return m_cubeMaterial.ready() && m_cubeMesh.ready();
}

EntityId Demo::constructCube()
{
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

  sysRender3d.renderer().compileShader(false, m_meshFeatures, m_materialFeatures);

  auto model = std::make_unique<DModel>();

  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_cubeMesh,
      .meshFeatures = m_meshFeatures,
      .material = m_cubeMaterial,
      .materialFeatures = m_materialFeatures,
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  sysRender3d.addEntity(id, std::move(model));

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

  MaterialFeatureSet materialFeatures{
    .flags{
      bitflag(MaterialFeatures::HasTexture)
    }
  };
  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.handle =
    m_engine.renderResourceLoader().loadTextureAsync("textures/fonts.png").wait();

  DText render{
    .scissor = 0,
    .material = m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)).wait(),
    .materialFeatures = materialFeatures,
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
  rotateCube();
  m_engine.update({});

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
