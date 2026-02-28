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
    Vec3f m_cubeInitialPosition = metresToWorldUnits(Vec3f{ 0.f, 2.f, -8.f });
    bool m_physicsActive = false;

    EntityId constructLight();
    EntityId constructCube();
    EntityId constructGround();
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
  m_cube = constructCube();
  constructGround();
  m_caption = constructCaption();

  auto& camera = m_engine.ecs().system<SysRender3d>().camera();
  camera.setPosition(metresToWorldUnits(Vec3f{ 0.f, 1.f, 0.f }));
}

EntityId Demo::constructCube()
{
  auto material = m_factory->createMaterial("textures/bricks.png");
  auto size = metresToWorldUnits(Vec3f{ 1.f, 1.f, 1.f });
  auto texSize = metresToWorldUnits(Vec2f{ 1.f, 1.f });
  auto id = m_factory->createCuboid(size, material, texSize, 0.f);

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto transform = translationMatrix4x4(m_cubeInitialPosition);
  sysSpatial.setEntityTransform(id, transform);

  return id;
}

EntityId Demo::constructGround()
{
  auto material = m_factory->createMaterial("textures/ground.png").wait();

  auto id = m_factory->createCuboid(metresToWorldUnits(Vec3f{ 50.f, 10.f, 50.f }), material,
    metresToWorldUnits(Vec2f{ 5.f, 5.f }), 0.f);

  m_engine.ecs().componentStore().component<CLocalTransform>(id).transform =
    translationMatrix4x4(metresToWorldUnits(Vec3f{0.f, -5.f, 0.f }));

  return id;
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  DSpatial spatial{
    .transform = translationMatrix4x4(metresToWorldUnits(Vec3f{ 5.f, 5.f, 2.f })),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
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
  m_engine.ecs().system<SysCollision>().setInverseMass(m_cube, 1.f);
  m_physicsActive = true;
}

void Demo::resetState()
{
  auto t = translationMatrix4x4(m_cubeInitialPosition);

  m_engine.ecs().system<SysCollision>().setInverseMass(m_cube, 0.f);
  m_engine.ecs().system<SysCollision>().setStationary(m_cube);
  m_engine.ecs().system<SysSpatial>().setEntityTransform(m_cube, t);

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
