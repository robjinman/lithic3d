#pragma once

#include "resource_manager.hpp"
#include <filesystem>

namespace lithic3d
{

struct TerrainTextureMap
{
  ResourceId mapTexture = NULL_RESOURCE_ID;
  ResourceId redTexture = NULL_RESOURCE_ID;
  ResourceId greenTexture = NULL_RESOURCE_ID;
  ResourceId blueTexture = NULL_RESOURCE_ID;
  ResourceId alphaTexture = NULL_RESOURCE_ID;
};

struct TerrainOptions
{
  ResourceId heightMap = NULL_RESOURCE_ID;
  std::map<std::string, TerrainTextureMap> textureMaps;
};

class TerrainBuilder
{
  public:
    virtual void loadHeightMap(const std::filesystem::path& path) = 0;

    virtual ~TerrainBuilder() = default;
};

using TerrainBuilderPtr = std::unique_ptr<TerrainBuilder>;

TerrainBuilderPtr createTerrainBuilder(const TerrainOptions& options);

} // namespace lithic3d
