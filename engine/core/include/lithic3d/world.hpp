#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
#include <memory>

namespace lithic3d
{

struct WorldCell
{
  std::vector<ResourceId> layers;
};

struct WorldGrid
{
  Vec2i sizeCells;              // Size in grid cell
  Vec2f sizeMetres;             // Size in metres
  std::vector<WorldCell> cells; // Row-major order
};

struct World
{
  WorldGrid grid;
};

using WorldPtr = std::unique_ptr<World>;

class FileSystem;
class TerrainBuilder;
class EntityFactory;

WorldPtr createWorld(const std::string& name, FileSystem& fileSystem,
  TerrainBuilder& terrainBuilder, EntityFactory& entityFactory, ResourceManager& resourceManager);

} // namespace lithic3d
