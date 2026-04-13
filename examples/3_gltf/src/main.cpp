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
    void onKeyDown(KeyboardKey) override {}
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
    EntityId m_model = NULL_ENTITY_ID;

    EntityId constructLight();
    EntityId constructModel();
    EntityId constructCaption();
    void rotateModel();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  constructLight();
  m_model = constructModel();
  constructCaption();
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  auto pos = metresToWorldUnits(Vec3f{ 0.5f, 0.5f, 0.2f });

  DSpatial spatial{
    .transform = translationMatrix4x4(pos),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 0.9f, 0.9f };
  light->ambient = 0.4f;
  light->specular = 0.9f;

  m_engine.ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

EntityId Demo::constructModel()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().idGen().getNewEntityId(); 
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  auto render = std::make_unique<DModel>();
  render->model = m_engine.modelLoader().loadModelAsync("monkey.gltf").wait();

  sysRender3d.addEntity(id, std::move(render));

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
    .enabled = true,
    .aabb{}
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = 0,
    .material = m_factory->createMaterialAsync("fonts.png").wait(),
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

void Demo::rotateModel()
{
  float a = (2 * PIf / 360.f) * (m_engine.currentTick() % 360);
  float b = (2 * PIf / 720.f) * (m_engine.currentTick() % 720);
  auto m = createTransform(metresToWorldUnits(Vec3f({ 0.f, 0.f, -0.5f })), { b, a, 0.f });

  m_engine.ecs().system<SysSpatial>().setLocalTransform(m_model, m);
}

bool Demo::update()
{
  rotateModel();
  m_engine.update({});

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - GLTF",
    .appShortName = "gltf",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080,
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
