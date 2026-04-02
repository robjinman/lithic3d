#pragma once

#include "resource_manager.hpp"
#include "entity_id.hpp"
#include "xml.hpp"

namespace lithic3d
{

struct TerrainConfig
{
  std::string world;

  // In metres
  float minHeight = 0.f;
  float maxHeight = 100.f;

  // In metres
  float cellWidth = 100.f;
  float cellHeight = 100.f;

  float waterLevel = 5.0f;
};

class TerrainBuilder
{
  public:
    virtual ResourceHandle loadTerrainRegionAsync(uint32_t x, uint32_t y,
      XmlNodePtr terrainXml) = 0;

    // Call only once handle returned by loadTerrainRegionAsync is ready
    virtual std::vector<EntityId> createEntities(ResourceId regionId) = 0;

    virtual ~TerrainBuilder() = default;
};

using TerrainBuilderPtr = std::unique_ptr<TerrainBuilder>;

class Ecs;
class FileSystem;
class ModelLoader;
class RenderResourceLoader;

TerrainBuilderPtr createTerrainBuilder(const TerrainConfig& config, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, FileSystem& fileSystem, Logger& logger);

} // namespace lithic3d
