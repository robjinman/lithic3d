#include "player.hpp"
#include <lithic3d/lithic3d.hpp>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

const float GRAVITY_STRENGTH = 3.5f;
const float MOUSE_LOOK_SPEED = 3.5f;
const float GRAVITATIONAL_CONSTANT =
  GRAVITY_STRENGTH * metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND);

class Demo : public Game
{
  public:
    Demo(Engine& engine);

    float gameViewportAspectRatio() const override { return 1.4f; }
    void onKeyDown(KeyboardKey) override;
    void onKeyUp(KeyboardKey) override;
    void onButtonDown(GamepadButton) override {}
    void onButtonUp(GamepadButton) override {}
    void onMouseButtonDown() override;
    void onMouseButtonUp() override;
    void onMouseMove(const Vec2f&, const Vec2f&) override;
    void onLeftStickMove(const Vec2f&) override;
    void onRightStickMove(const Vec2f&) override;
    void onWindowResize(uint32_t, uint32_t) override {}
    void hideMobileControls() override {}
    void showMobileControls() override {}
    bool update() override;

  private:
    Engine& m_engine;
    WorldLoaderPtr m_worldLoader;
    WorldGridPtr m_worldGrid;
    FactoryPtr m_factory;
    EntityFactoryPtr m_entityFactory;
    PlayerPtr m_player;
    EntityId m_light;
    EntityId m_caption;
    InputState m_inputState;  // TODO: Engine to maintain input state?
    Vec2f m_leftStickDelta;
    Vec2f m_mouseDelta;
    float m_playerVerticalVelocity = 0.f;
    bool m_freeflyMode = false;

    EntityId constructLight();
    EntityId constructCaption();
    void processMouseInput();
    void processKeyboardInput();
    void gravity();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_entityFactory = createEntityFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader(), m_engine.resourceManager(), m_engine.fileSystem(),
    m_engine.logger());

  m_worldLoader = createWorldLoader(m_engine.ecs(), m_engine.fileSystem(), *m_entityFactory,
    m_engine.modelLoader(), m_engine.renderResourceLoader(), m_engine.resourceManager(),
    m_engine.logger());

  WorldTraversalOptions options{
    .sliceLoadDistances = { 1, 1, 1, 1, 1, 1 }
  };

  m_worldGrid = createWorldGrid(options, *m_worldLoader, m_engine.eventSystem(), m_engine.logger());

  m_factory = createFactory(m_engine.ecs(), m_engine.modelLoader(),
    m_engine.renderResourceLoader());

  m_player = createPlayer(m_engine.ecs().system<SysRender3d>().camera());

  m_light = constructLight();
  m_caption = constructCaption();

  m_player->setPosition({ 0.5f, 0.f, 0.5f });
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
  material->textures = {
    m_engine.renderResourceLoader().loadTextureAsync("textures/fonts.png").wait()
  };

  DText render{
    .scissor = 0,
    .material = m_engine.renderResourceLoader().loadMaterialAsync(std::move(material)).wait(),
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

void Demo::processKeyboardInput()
{
  Vec3f direction{};
  float speed = m_player->getSpeed();
  const auto forward = m_player->getDirection();
  const auto strafe = m_player->getDirection().cross(Vec3f{0, 1, 0});

  if (m_leftStickDelta != Vec2f{}) {
    float threshold = 0.4f;

    if (m_leftStickDelta.magnitude() > threshold) {
      float x = m_leftStickDelta[0];
      float y = -m_leftStickDelta[1];
      direction = forward * y + strafe * x;
      speed = m_leftStickDelta.magnitude() * m_player->getSpeed();
    }

    m_leftStickDelta = Vec2f{};
  }
  else {
    if (m_inputState.keysPressed.contains(KeyboardKey::W)) {
      direction += forward;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::S)) {
      direction -= forward;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::D)) {
      direction += strafe;
    }
    if (m_inputState.keysPressed.contains(KeyboardKey::A)) {
      direction -= strafe;
    }
  }

  if (m_inputState.keysPressed.contains(KeyboardKey::E)) {
    if (m_player->getPosition()[1] == 0) {
      m_playerVerticalVelocity = sqrt(m_player->getJumpHeight() * 2.f * GRAVITATIONAL_CONSTANT);
    }
  }
  
  if (direction != Vec3f{}) {
    if (!m_freeflyMode) {
      direction[1] = 0;
    }

    direction = direction.normalise();
    auto delta = direction * speed / static_cast<float>(TICKS_PER_SECOND);

    m_player->translate(delta);
  }
}

void Demo::processMouseInput()
{
  m_player->rotate(-MOUSE_LOOK_SPEED * m_mouseDelta[1], -MOUSE_LOOK_SPEED * m_mouseDelta[0]);
  m_mouseDelta = Vec2f{};
}

void Demo::gravity()
{
  m_player->translate(Vec3f{ 0, m_playerVerticalVelocity, 0 });
  float altitude = m_player->getPosition()[1];

  if (altitude > 0) {
    m_playerVerticalVelocity =
      std::max(m_playerVerticalVelocity - GRAVITATIONAL_CONSTANT, -altitude);
  }
  else if (altitude == 0) {
    m_playerVerticalVelocity = 0;
  }
}

bool Demo::update()
{
  processKeyboardInput();
  processMouseInput();
  gravity();
  m_engine.update(m_inputState);
  m_worldGrid->update(m_player->getPosition());

  return true;
}

void Demo::onKeyDown(KeyboardKey key)
{
  m_inputState.keysPressed.insert(key);

  if (key == KeyboardKey::F) {
    m_engine.logger().info(STR("Simulation tick rate: " << m_engine.measuredTickRate()));
  }
}

void Demo::onKeyUp(KeyboardKey key)
{
  m_inputState.keysPressed.erase(key);
}

void Demo::onMouseButtonDown()
{
  m_inputState.mouseButtonsPressed.insert(MouseButton::Left);
}

void Demo::onMouseButtonUp()
{
  m_inputState.mouseButtonsPressed.erase(MouseButton::Left);
}

void Demo::onMouseMove(const Vec2f& pos, const Vec2f& delta)
{
  m_inputState.mouseX = pos[0];
  m_inputState.mouseY = pos[1];
  m_mouseDelta = delta;
}

void Demo::onLeftStickMove(const Vec2f& delta)
{

}

void Demo::onRightStickMove(const Vec2f& delta)
{
  const float threshold = 0.1;
  const float sensitivity = 0.01;

  if (delta.squareMagnitude() >= threshold) {
    m_inputState.mouseX += delta[0] * sensitivity;
    m_inputState.mouseY += delta[1] * -sensitivity;
  }
}

} // namespace

GameConfig getGameConfig()
{
  return {
    .appDisplayName = "Lithic3D Demo - Terrain",
    .appShortName = "terrain",
    .vendorShortName = "freeholdapps",
    .windowW = 640,
    .windowH = 480,
    .fullscreenResolutionW = 1920,
    .fullscreenResolutionH = 1080,
    .captureMouse = true,
    .shaderManifest = "shaders.xml"
  };
}

GamePtr createGame(Engine& engine)
{
  return std::make_unique<Demo>(engine);
}
