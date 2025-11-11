#include <fge/engine.hpp>
#include <fge/game.hpp>
#include <fge/renderer.hpp>
#include <fge/ecs.hpp>
#include <fge/file_system.hpp>
#include <fge/sys_render_2d.hpp>
#include <fge/sys_render_3d.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/logger.hpp>

using namespace fge;

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

    Tick m_currentTick = 0; // TODO: Remove

    EntityId constructCube();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_cube = constructCube();

  m_engine.renderer().start(); // TODO: Move to engine?
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
  // TODO: Replace with m_engine.update()
  m_engine.ecs().update(m_currentTick++, m_inputState);
  m_engine.eventSystem().processEvents();

  rotateCube();

  return true;
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "FGE Minimal Example",
    .appShortName = "minimal",
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
