#pragma once

#include "resource_manager.hpp"
#include "sys_collision.hpp"
#include "xml.hpp"

namespace lithic3d
{

struct TerrainChunk
{
  HeightMap heightMap;
  Vec3f offset;
  Vec3f dimensions;
  ResourceHandle model;
};

struct TerrainPiece
{
  Mat4x4f transform = identityMatrix<4>();
  std::vector<TerrainChunk> chunks;

  EntityId entityId = NULL_ENTITY_ID;
  bool inverted = false;
  std::filesystem::path heightMapFile;
  std::filesystem::path splatMapFile;
  std::array<std::filesystem::path, 4> splatTextures;
  uint32_t cellX = 0;
  uint32_t cellY = 0;
};

class TerrainBuilder
{
  public:
    virtual ResourceHandle loadTerrainRegionAsync(uint32_t x, uint32_t y,
      XmlNodePtr terrainXml) = 0;

    // Call only once handle returned by loadTerrainRegionAsync is ready
    virtual std::vector<EntityId> createEntities(EntityId parentId, ResourceId regionId) = 0;

    virtual const TerrainPiece& getTerrainPiece(EntityId id) const = 0;
    virtual void reloadTerrainRegion(ResourceId regionId) = 0;

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
