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
    EntityId m_cube = NULL_ENTITY;
    ResourceId m_cubeResourceId = NULL_RESOURCE_ID;

    void constructLight();
    void constructCube();
    void constructCaption();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_factory = createFactory(m_engine.ecs(), m_engine.fileSystem());

  constructLight();
  constructCube();
  constructCaption();

  m_engine.renderer().start(); // TODO: Move to engine?
}

template<typename T>
struct Handle : public ResourceHandle
{
  Handle(const T& h)
    : handle(h)
  {}

  T handle;
};

using TextureRHandle = Handle<RenderItemId>;
using MaterialRHandle = Handle<render::MaterialHandle>;
using MeshRHandle = Handle<render::MeshHandle>;
using EntityRHandle = Handle<EntityId>;

void Demo::constructCube()
{
  auto& resourceManager = m_engine.resourceManager();

  Resource texture{
    .id = resourceManager.nextResourceId(),
    .loader = [this](const ResourceHandleList&) {
      auto texture = loadTexture(m_engine.fileSystem().readAppDataFile("textures/bricks.png"));
      auto textureId = m_engine.renderer().addTexture(std::move(texture));
      return std::make_unique<TextureRHandle>(textureId);
    },
    .unloader = [this](const ResourceHandle& h) {
      m_engine.renderer().removeTexture(dynamic_cast<const TextureRHandle&>(h).handle);
    },
    .dependencies = {}
  };

  Resource material{
    .id = resourceManager.nextResourceId(),
    .loader = [this](const ResourceHandleList& deps) {
      render::MaterialFeatureSet features{
        .flags{
          bitflag(render::MaterialFeatures::HasTexture)
        }
      };
      auto material = std::make_unique<Material>(features);
      material->texture.id = dynamic_cast<const TextureRHandle&>(deps[0].get()).handle;
      auto handle = m_engine.renderer().addMaterial(std::move(material));
      return std::make_unique<MaterialRHandle>(handle);
    },
    .unloader = [this](const ResourceHandle& h) {
      m_engine.renderer().removeMaterial(dynamic_cast<const MaterialRHandle&>(h).handle.id);
    },
    .dependencies = { texture.id }
  };

  Resource entity{
    .id = resourceManager.nextResourceId(),
    .loader = [this](const ResourceHandleList& deps) {
      EntityId id = m_engine.ecs().componentStore().allocate<DSpatial, DModel>();

      auto material = dynamic_cast<const MaterialRHandle&>(deps[0].get()).handle;
      m_factory->constructCuboid(id, material, { 1.f, 1.f, 1.f }, { 1.f, 1.f });

      return std::make_unique<EntityRHandle>(id);
    },
    .unloader = [this](const ResourceHandle& h) {
      m_engine.ecs().removeEntity(dynamic_cast<const EntityRHandle&>(h).handle);
    },
    .dependencies = { material.id }
  };

  resourceManager.addResource(texture);
  resourceManager.addResource(material);
  resourceManager.addResource(entity);
  resourceManager.loadResources({ entity.id }).get();

  m_cubeResourceId = entity.id;
  m_cube = dynamic_cast<const EntityRHandle&>(resourceManager.getHandle(entity.id)).handle;
}

void Demo::constructLight()
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
}

void Demo::constructCaption()
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
}

void Demo::onKeyDown(KeyboardKey key)
{
  if (key == KeyboardKey::Space) {
    if (m_cube != NULL_ENTITY) {
      m_engine.resourceManager().unloadResources({ m_cubeResourceId });
      m_cube = NULL_ENTITY;
    }
    else {
      m_engine.resourceManager().loadResources({ m_cubeResourceId }).get();
      m_cube = dynamic_cast<const EntityRHandle&>(m_engine.resourceManager().getHandle(m_cubeResourceId)).handle;
    }
  }
}

void Demo::rotateCube()
{
  if (m_cube == NULL_ENTITY) {
    return;
  }

  float a = (2 * PIf / 360.f) * (m_engine.currentTick() % 360);
  float b = (2 * PIf / 720.f) * (m_engine.currentTick() % 720);

  m_engine.ecs().system<SysSpatial>().setEntityTransform(m_cube,
    createTransform({ 0.f, 0.f, 5.f }, { b, a, 0.f }));
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
    .appDisplayName = "Lithic3D Demo - Resource Management",
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
