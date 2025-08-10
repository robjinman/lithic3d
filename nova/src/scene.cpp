#include "scene.hpp"
#include "spatial_system.hpp"
#include "render_system.hpp"

PlayerPtr createScene(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatial->transform = translationMatrix4x4(Vec3f{ 0.5f, 0.2f, 0.f });
  spatialSystem.addComponent(std::move(spatial));

  float_t atlasWidthPx = 1024.f;
  float_t atlasHeightPx = 512.f;
  float_t offsetXpx = 384.f;
  float_t offsetYpx = 256.f;
  float_t wPx = 32.f;
  float_t hPx = 48.f;
  float_t w = wPx / atlasWidthPx;
  float_t h = hPx / atlasHeightPx;

  auto render = std::make_unique<CRender>(id);
  render->textureRect = Rectf{
    .x = offsetXpx / atlasWidthPx,
    .y = (atlasHeightPx - offsetYpx - hPx) / atlasHeightPx,
    .w = w,
    .h = h
  };
  render->size = Vec2f{ 0.1f, 0.1f };

  renderSystem.addComponent(std::move(render));
}
