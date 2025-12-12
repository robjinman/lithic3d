#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
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
    virtual void createEntities(ResourceId cellSliceId) = 0;

    // To unload a cell slice, delete the entities first, then delete the slice handle

    virtual ~WorldLoader() = default;
};

using WorldLoaderPtr = std::unique_ptr<WorldLoader>;

class FileSystem;
class EntityFactory;
class RenderResourceLoader;

WorldLoaderPtr createWorldLoader(FileSystem& fileSystem, EntityFactory& entityFactory,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager);

} // namespace lithic3d
