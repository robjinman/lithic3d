#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
#include "entity_id.hpp"
#include <memory>

namespace lithic3d
{

struct WorldInfo
{
  uint32_t gridWidth = 0;
  uint32_t gridHeight = 0;
  float cellWidth = 0;
  float cellHeight = 0;
};

class WorldLoader
{
  public:
    virtual const WorldInfo& worldInfo() const = 0;

    virtual ResourceHandle loadCellSliceAsync(uint32_t x, uint32_t y, uint32_t sliceIdx) = 0;

    // Call only once handle returned by loadCellSliceAsync is ready
    virtual std::vector<EntityId> createEntities(ResourceId cellSliceId) = 0;

    // To unload a cell slice, delete the entities first, then delete the slice handle

    virtual ~WorldLoader() = default;
};

using WorldLoaderPtr = std::unique_ptr<WorldLoader>;

class Ecs;
class FileSystem;
class EntityFactory;
class ModelLoader;
class RenderResourceLoader;

WorldLoaderPtr createWorldLoader(Ecs& ecs, FileSystem& fileSystem, EntityFactory& entityFactory,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, Logger& logger);

} // namespace lithic3d
