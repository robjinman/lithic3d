#include <lithic3d/lithic3d.hpp>

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
    EntityId m_parent = NULL_ENTITY_ID;
    EntityId m_child = NULL_ENTITY_ID;

    EntityId constructLight();
    EntityId constructParent(ResourceHandle model);
    EntityId constructChild(ResourceHandle model);
    EntityId constructCaption();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  auto camPos = metresToWorldUnits(Vec3f{ 0.f, 0.f, 0.f });
  m_engine.ecs().system<SysRender3d>().camera().setPosition(camPos);

  auto model = m_engine.modelLoader().loadModelAsync("models/indicator.gltf").wait();

  constructLight();
  m_parent = constructParent(model);
  m_child = constructChild(model);
  constructCaption();

  auto pos = translationMatrix4x4(metresToWorldUnits(Vec3f{-0.5f, 1.f, -3.f}));
  m_engine.ecs().system<SysSpatial>().transformEntityLocal(m_parent, pos);
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

EntityId Demo::constructParent(ResourceHandle model)
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().idGen().getNewEntityId(); 
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  auto render = std::make_unique<DModel>();
  render->model = model;

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

EntityId Demo::constructChild(ResourceHandle model)
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().idGen().getNewEntityId(); 
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  DSpatial spatial{};
  spatial.parent = m_parent;

  sysSpatial.addEntity(id, spatial);

  auto render = std::make_unique<DModel>();
  render->model = model;

  sysRender3d.addEntity(id, std::move(render));

  auto pos = metresToWorldUnits(Vec3f{ 1.f, 0.f, 0.f });
  auto t = createTransform(pos, {}, { 0.5f, 0.5f, 0.5f });
  sysSpatial.setLocalTransform(id, t);

  return id;
}

EntityId Demo::constructCaption()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender2d = m_engine.ecs().system<SysRender2d>();

  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DText>(id);

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
    .parent = sysSpatial.root(),
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = 0,
    .material = m_factory->createMaterialAsync("textures/fonts.png").wait(),
    .textureRect = {
      .x = pxToUvX(768.f, 1024.f),
      .y = pxToUvY(0.f, 256.f, 256.f),
      .w = pxToUvW(256.f, 1024.f),
      .h = pxToUvH(256.f, 256.f)
    },
    .text = "Welcome to Lithic3D!",
    .zIndex = 0,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  sysRender2d.addEntity(id, render);

  return id;
}

void Demo::onKeyDown(KeyboardKey key)
{
  // TODO
}

bool Demo::update()
{
  m_engine.update({});

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  Tick timeStep = TICKS_PER_SECOND / 2;

  Tick tick = m_engine.currentTick();
  if (tick % timeStep == 0) {
    if (tick < 10 * timeStep) {
      sysSpatial.transformEntitySelf(m_parent, rotationMatrix4x4(Vec3f{ degreesToRadians(10.f), 0.f, 0.f }));
    }
  }

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Transforms",
    .appShortName = "transforms",
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
