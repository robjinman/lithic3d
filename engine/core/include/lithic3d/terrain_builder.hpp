#pragma once

#include "resource_manager.hpp"
#include "entity_id.hpp"
#include "xml.hpp"
#include "math.hpp"

namespace lithic3d
{

class TerrainBuilder
{
  public:
    virtual ResourceHandle loadTerrainRegionAsync(uint32_t x, uint32_t y,
      XmlNodePtr terrainXml) = 0;

    // Call only once handle returned by loadTerrainRegionAsync is ready
    virtual std::vector<EntityId> createEntities(EntityId parentId, ResourceId regionId) = 0;

    virtual ~TerrainBuilder() = default;
};

using TerrainBuilderPtr = std::unique_ptr<TerrainBuilder>;

class Ecs;
class ModelLoader;
class RenderResourceLoader;
struct GameDataPaths;

TerrainBuilderPtr createTerrainBuilder(const Vec2f& cellSizeMetres, Ecs& ecs,
  ModelLoader& modelLoader, RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const GameDataPaths& paths, Logger& logger);

} // namespace lithic3d
