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

    virtual ~WorldLoader() = default;
};

using WorldLoaderPtr = std::unique_ptr<WorldLoader>;

class FileSystem;
class EntityFactory;

WorldLoaderPtr createWorldLoader(FileSystem& fileSystem, EntityFactory& entityFactory,
  ResourceManager& resourceManager);

} // namespace lithic3d
