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
    EntityId m_cube = NULL_ENTITY;

    EntityId constructLight();
    EntityId constructCube();
    EntityId constructCaption();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.fileSystem());

  constructLight();
  m_cube = constructCube();
  constructCaption();

  m_engine.renderer().start(); // TODO: Move to engine?
}

EntityId Demo::constructCube()
{
  EntityId id = m_engine.ecs().componentStore().allocate<DSpatial, DModel>();

  auto material = m_factory->constructMaterial("textures/bricks.png");
  m_factory->constructCuboid(id, material, { 1.f, 1.f, 1.f }, { 1.f, 1.f });

  return id;
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

EntityId Demo::constructCaption()
{
  auto id = m_engine.ecs().componentStore().allocate<DSpatial, DText>();

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
    .parent = m_engine.ecs().system<SysSpatial>().root(),
    .enabled = true
  };

  m_engine.ecs().system<SysSpatial>().addEntity(id, spatial);

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

  m_engine.ecs().system<SysRender2d>().addEntity(id, render);

  return id;
}

void Demo::rotateCube()
{
  float a = (2 * PIf / 360.f) * (m_engine.currentTick() % 360);
  float b = (2 * PIf / 720.f) * (m_engine.currentTick() % 720);

  // TODO
  m_engine.ecs().componentStore().component<CLocalTransform>(m_cube).transform =
    translationMatrix4x4(Vec3f{ 0.f, 0.f, 5.f }) *
    rotationMatrix4x4(Vec3f{ b, a, 0.f });
}

bool Demo::update()
{
  rotateCube();
  m_engine.update({});

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Cube",
    .appShortName = "cube",
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
