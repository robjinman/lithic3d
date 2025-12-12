#pragma once

#include "resource_manager.hpp"
#include "renderables.hpp"
#include <filesystem>
#include <map>

namespace lithic3d
{

struct TerrainMaterialMap
{
  ResourceHandle mapMaterial;
  ResourceHandle redMaterial;
  ResourceHandle greenMaterial;
  ResourceHandle blueMaterial;
  ResourceHandle alphaMaterial;
};

struct TerrainConfig
{
  render::TexturePtr heightMap;
  TerrainMaterialMap materialMaps;  // TODO: Support multiple
};

class TerrainBuilder
{
  public:
    virtual ~TerrainBuilder() = default;
};

using TerrainBuilderPtr = std::unique_ptr<TerrainBuilder>;

TerrainBuilderPtr createTerrainBuilder(const TerrainConfig& config);

} // namespace lithic3d
