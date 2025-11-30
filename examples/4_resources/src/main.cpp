#include <lithic3d/lithic3d.hpp>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

struct MaterialDependencies
{
  ResourceHandle textureHandle;
  ResourceHandle normalMapHandle;
  ResourceHandle cubeMapHandle;
};

class RenderResourceLoader : private ResourceProvider
{
  public:
    RenderResourceLoader(ResourceManager& resourceManager, FileSystem& fileSystem,
      Renderer& renderer);

    ResourceHandle loadTextureAsync(const fs::path& path);
    ResourceHandle loadMaterialAsync(MaterialPtr material);
    ResourceHandle loadMeshAsync(MeshPtr mesh);

  private:
    ResourceManager& m_resourceManager;
    FileSystem& m_fileSystem;
    Renderer& m_renderer;
};

RenderResourceLoader::RenderResourceLoader(ResourceManager& resourceManager, FileSystem& fileSystem,
  Renderer& renderer)
  : m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
  , m_renderer(renderer)
{}

using RenderResourceLoaderPtr = std::unique_ptr<RenderResourceLoader>;

ResourceHandle RenderResourceLoader::loadTextureAsync(const fs::path& path)
{
  return m_resourceManager.loadResource([this, path](ResourceId id) {
    auto data = m_fileSystem.readAppDataFile(path);
    auto texture = loadTexture(data);
    m_renderer.addTexture(id, std::move(texture));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeTexture(id);
      }
    };
  });
}

ResourceHandle RenderResourceLoader::loadMaterialAsync(MaterialPtr material)
{/*
  return m_resourceManager.loadResource([this, material = std::move(material)](ResourceId id) mutable {
    m_renderer.addMaterial(id, std::move(material));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeMaterial(id);
      }
    };
  });*/
}

ResourceHandle RenderResourceLoader::loadMeshAsync(MeshPtr mesh)
{/*
  return m_resourceManager.loadResource([this, mesh = std::move(mesh)](ResourceId id) mutable {
    m_renderer.addMesh(id, std::move(mesh));

    return ManagedResource{
      .provider = weak_from_this(),
      .unloader = [this](ResourceId id) {
        m_renderer.removeMesh(id);
      }
    };
  });*/
}

class Demo : public Game, private ResourceProvider
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
    RenderResourceLoaderPtr m_renderResourceLoader;
    EntityId m_light;
    EntityId m_cube;
    EntityId m_caption;

    EntityId constructLight();
    EntityId constructCube();
    EntityId constructCaption();
    void rotateCube();
};

Demo::Demo(Engine& engine)
  : m_engine(engine)
{
  m_renderResourceLoader = std::make_unique<RenderResourceLoader>(m_engine.resourceManager(),
    m_engine.fileSystem(), m_engine.renderer());

  m_light = constructLight();
  m_cube = constructCube();
  //m_caption = constructCaption();

  m_engine.renderer().start(); // TODO: Move to engine?
}

EntityId Demo::constructCube()
{
  MaterialFeatureSet materialFeatures{
    .flags{
      bitflag(MaterialFeatures::HasTexture)
    }
  };
  auto material = std::make_unique<Material>(materialFeatures);
  material->texture.handle = m_renderResourceLoader->loadTextureAsync("textures/bricks.png");

  auto id = m_engine.ecs().idGen().getNewEntityId();
  m_engine.ecs().componentStore().allocate<DSpatial, DModel>(id);

  Vec3f size{ 1.f, 1.f, 1.f };
  Vec2f materialSize{ 1.f, 1.f };

  auto& sysSpatial = m_engine.ecs().system<SysSpatial>();
  auto& sysRender3d = m_engine.ecs().system<SysRender3d>();

  DSpatial spatial{};
  spatial.parent = sysSpatial.root();
  spatial.aabb = {
    .min = -size * 0.5f,
    .max = size * 0.5f
  };

  sysSpatial.addEntity(id, spatial);

  MeshFeatureSet meshFeatures{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };

  auto mesh = render::cuboid(size[0], size[1], size[2], materialSize);
  mesh->featureSet = meshFeatures;

  sysRender3d.renderer().compileShader(false, mesh->featureSet, materialFeatures);

  auto model = std::make_unique<DModel>();

  model->submodels.push_back(
    std::unique_ptr<Submodel>(new Submodel{
      .mesh = m_renderResourceLoader->loadMeshAsync(std::move(mesh)).wait(), // TODO: Remove wait
      .meshFeatures = meshFeatures,
      .material = m_renderResourceLoader->loadMaterialAsync(std::move(material)).wait(), // TODO: Remove wait
      .materialFeatures = materialFeatures,
      .skin = nullptr,
      .jointTransforms{}
    })
  );

  sysRender3d.addEntity(id, std::move(model));
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

  DText render{
    .scissor = 0,
    .material = 0,//m_renderResourceLoader->constructMaterial("textures/fonts.png"),
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

void Demo::onKeyDown(KeyboardKey key)
{
  if (key == KeyboardKey::Space) {
    // TODO
  }
}

void Demo::rotateCube()
{
  if (m_cube == NULL_ENTITY_ID) {
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
