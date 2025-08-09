#include "scene.hpp"
#include "spatial_system.hpp"
#include "render_system.hpp"

PlayerPtr createScene(SpatialSystem& spatialSystem, RenderSystem& renderSystem,
  const FileSystem& fileSystem, Logger& logger)
{
  auto id = System::nextId();

  auto spatial = std::make_unique<CSpatial>(id);
  spatialSystem.addComponent(std::move(spatial));

  auto render = std::make_unique<CRender>(id);
  render->offset = Vec2f{ 0.f, 0.f };
  render->size = Vec2f{ 1.f, 1.f };

  renderSystem.addComponent(std::move(render));
}
