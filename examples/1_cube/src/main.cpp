#include <lithic3d/engine.hpp>
#include <lithic3d/game.hpp>
#include <lithic3d/renderer.hpp>
#include <lithic3d/ecs.hpp>
#include <lithic3d/file_system.hpp>
#include <lithic3d/sys_render_2d.hpp>
#include <lithic3d/sys_render_3d.hpp>
#include <lithic3d/sys_spatial.hpp>
#include <lithic3d/logger.hpp>

using namespace lithic3d;

namespace
{

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override;
    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onButtonDown(GamepadButton button) override;
    void onButtonUp(GamepadButton button) override;
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f& pos, const Vec2f& delta) override;
    void onLeftStickMove(const Vec2f& delta) override;
    void onRightStickMove(const Vec2f& delta) override;
    void onWindowResize(uint32_t w, uint32_t h) override;
    void hideMobileControls() override;
    void showMobileControls() override;
    bool update() override;

  private:
    Engine& m_engine;
    InputState m_inputState;
    EntityId m_cube = NULL_ENTITY;

    Tick m_currentTick = 0;

    EntityId constructLight();
    EntityId constructCube();
    EntityId constructCaption();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  constructLight();
  m_cube = constructCube();
  constructCaption();

  m_engine.renderer().start(); // TODO: Move to engine?
}

EntityId Demo::constructLight()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  auto id = m_engine.ecs().componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform
  >();

  DSpatial spatial{
    .transform = translationMatrix4x4(Vec3f{ 5.f, 5.f, 2.f }),
    .parent = sysSpatial.root(),
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  float_t size = metresToWorldUnits(0.2);
  Vec3f colour{ 1.f, 0.9f, 0.9f };

  auto light = std::make_unique<DLight>();
  light->colour = colour;
  light->ambient = 0.4f;
  light->specular = 0.9f;

  sysRender3d.addEntity(id, std::move(light));

  return id;
}

EntityId Demo::constructCube()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  EntityId id = m_engine.ecs().componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform
  >();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();

  sysSpatial.addEntity(id, spatial);

  auto mesh = render::cuboid(1.f, 1.f, 1.f, { 1.f, 1.f });

  mesh->featureSet = render::MeshFeatureSet{
    .vertexLayout = {
      render::BufferUsage::AttrPosition,
      render::BufferUsage::AttrNormal,
      render::BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  render::MaterialFeatures::Flags materialFlags{};
  materialFlags.set(render::MaterialFeatures::HasTexture);

  render::MaterialFeatureSet materialFeatures{
    .flags = materialFlags
  };

  m_engine.renderer().compileShader(false, mesh->featureSet, materialFeatures);

  auto texture = render::loadTexture(m_engine.fileSystem().readAppDataFile("textures/bricks.png"));
  auto material = std::make_unique<render::Material>(materialFeatures);
  material->texture.id = sysRender3d.addTexture(std::move(texture));

  auto model = std::make_unique<DModel>();

  model->submodels.push_back(
    Submodel{
      .mesh = sysRender3d.addMesh(std::move(mesh)),
      .material = sysRender3d.addMaterial(std::move(material)),
      .skin = nullptr
    }
  );

  sysRender3d.addEntity(id, std::move(model));

  return id;
}

EntityId Demo::constructCaption()
{
  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender2d = m_engine.ecs().system<SysRender2d>();

  EntityId id = m_engine.ecs().componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CRender2d, CSprite
  >();

  DSpatial spatial{
    .transform = screenSpaceTransform({ 0.15f, 0.2f }, { 0.05f, 0.1f }),
    .parent = sysSpatial.root(),
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = 0,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = "Welcome to Lithic3D!",
    .zIndex = 0,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  sysRender2d.addEntity(id, render);

  return id;
}

float Demo::gameViewportAspectRatio() const
{
  return 1.4;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_engine.logger().info("Key down");
  m_inputState.keysPressed.insert(key);
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_engine.logger().info("Key up");
  m_inputState.keysPressed.erase(key);
}

void Demo::onButtonDown(GamepadButton button)
{
  m_engine.logger().info("Button down");
  // Map to KeyboardKey
}

void Demo::onButtonUp(GamepadButton button)
{
  m_engine.logger().info("Button up");
  // Map to KeyboardKey
}

void Demo::onMouseButtonDown()
{
  m_engine.logger().info("Mouse button down");
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void Demo::onMouseButtonUp()
{
  m_engine.logger().info("Mouse button up");
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
}

void Demo::onLeftStickMove(const Vec2f& delta)
{
  m_engine.logger().info("Left stick move");
}

void Demo::onRightStickMove(const Vec2f& delta)
{
  m_engine.logger().info("Right stick move");
}

void Demo::onWindowResize(uint32_t w, uint32_t h)
{
  m_engine.logger().info("Window resize");
}

void Demo::hideMobileControls()
{
  m_engine.logger().info("Hide mobile controls");
}

void Demo::showMobileControls()
{
  m_engine.logger().info("Show mobile controls");
}

void Demo::rotateCube()
{
  float a = (2 * PIf / 360.f) * (m_currentTick % 360);
  float b = (2 * PIf / 720.f) * (m_currentTick % 720);

  m_engine.ecs().componentStore().component<CLocalTransform>(m_cube).transform =
    translationMatrix4x4(Vec3f{ 0.f, 0.f, -5.f }) *
    rotationMatrix4x4(Vec3f{ b, a, 0.f });
}

bool Demo::update()
{
  rotateCube();
  m_engine.update(m_currentTick++, m_inputState);

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
    .fullscreenResolutionH = 1080
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
