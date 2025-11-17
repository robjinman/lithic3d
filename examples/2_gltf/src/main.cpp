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
    EntityId m_model = NULL_ENTITY;

    EntityId constructLight();
    EntityId constructModel();
    EntityId constructCaption();
    void rotateModel();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.fileSystem());

  constructLight();
  m_model = constructModel();
  constructCaption();

  m_engine.renderer().start(); // TODO: Move to engine?
}

EntityId Demo::constructLight()
{
  auto id = m_engine.ecs().componentStore().allocate<DSpatial, DLight>();

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

EntityId Demo::constructModel()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().componentStore().allocate<DSpatial, DModel>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  auto modelData = m_engine.modelLoader().loadModelData("models/monkey.gltf");
  auto render = m_engine.modelLoader().createRenderComponent(std::move(modelData), false);

  for (auto& submodel : render->submodels) {
    submodel.mesh.features.flags.set(MeshFeatures::CastsShadow, false);
    sysRender3d.renderer().compileShader(false, submodel.mesh.features, submodel.material.features);
  }

  sysRender3d.addEntity(id, std::move(render));

  return id;
}

EntityId Demo::constructCaption()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender2d = m_engine.ecs().system<SysRender2d>();

  EntityId id = m_engine.ecs().componentStore().allocate<DSpatial, DText>();

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
    .parent = sysSpatial.root(),
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = 0,
    .material = m_factory->constructMaterial("textures/fonts.png"),
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

  // TODO
  m_engine.ecs().componentStore().component<CLocalTransform>(m_model).transform =
    translationMatrix4x4(Vec3f{ 0.f, 0.f, 5.f }) *
    rotationMatrix4x4(Vec3f{ b, a, 0.f });
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
    .captureMouse = false
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
