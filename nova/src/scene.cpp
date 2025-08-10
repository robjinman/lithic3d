#include "scene.hpp"
#include "spatial_system.hpp"
#include "render_system.hpp"

namespace
{

const float_t ATLAS_WIDTH_PX = 1024.f;
const float_t ATLAS_HEIGHT_PX = 512.f;

class SceneBuilder
{
  public:
    SceneBuilder(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
      const FileSystem& fileSystem, Logger& logger);

    void buildScene();

  private:
    Logger& m_logger;
    const FileSystem& m_fileSystem;
    SpatialSystem& m_spatialSystem;
    RenderSystem& m_renderSystem;

    void constructSky();
    void constructTrees();
    void constructFakeSoil();
    void constructPlayer();
};

SceneBuilder::SceneBuilder(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
  , m_spatialSystem(spatialSystem)
  , m_renderSystem(renderSystem)
{
}

void SceneBuilder::constructSky()
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.f, 0.75f, -10.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 416.f;
  float_t wPx = 128.f;
  float_t hPx = 32.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 1.3125f, 0.25f };

  m_renderSystem.addComponent(std::move(render));
}

void SceneBuilder::constructTrees()
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.f, 0.68375, -8.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 0.f;
  float_t offsetYpx = 352.f;
  float_t wPx = 256.f;
  float_t hPx = 40.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 1.3125f, 0.1875f };

  m_renderSystem.addComponent(std::move(render));
}

void SceneBuilder::constructFakeSoil()
{
  for (float_t x = 0.f; x <= 1.25; x += 0.0625) {
    auto id = System::nextId();

    auto spatial = std::make_unique<CSpatial>(id);
    spatial->transform = translationMatrix4x4(Vec3f{ x, 0.6875f, -9.f });
    m_spatialSystem.addComponent(std::move(spatial));

    float_t offsetXpx = 384.f;
    float_t offsetYpx = 0.f;
    float_t wPx = 16.f;
    float_t hPx = 16.f;
    float_t w = wPx / ATLAS_WIDTH_PX;
    float_t h = hPx / ATLAS_HEIGHT_PX;

    auto render = std::make_unique<CRender>(id);
    render->textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    };
    render->size = Vec2f{ 0.0625, 0.0625f };

    m_renderSystem.addComponent(std::move(render));
  }
}

void SceneBuilder::constructPlayer()
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.5f, 0.2f, 0.f });
  m_spatialSystem.addComponent(std::move(spatial));

  float_t offsetXpx = 384.f;
  float_t offsetYpx = 256.f;
  float_t wPx = 32.f;
  float_t hPx = 48.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  auto render = std::make_unique<CRender>(id);
  render->textureRect = Rectf{
    .x = offsetXpx / ATLAS_WIDTH_PX,
    .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 0.0625f, 0.0625f };

  m_renderSystem.addComponent(std::move(render));
}

void SceneBuilder::buildScene()
{
  constructSky();
  constructTrees();
  constructFakeSoil();
  constructPlayer();
}

}

PlayerPtr createScene(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  SceneBuilder builder{spatialSystem, renderSystem, fileSystem, logger};
  builder.buildScene();
}
