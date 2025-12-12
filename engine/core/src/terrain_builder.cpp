#include "lithic3d/terrain_builder.hpp"

namespace lithic3d
{
namespace
{

class TerrainBuilderImpl : public TerrainBuilder
{
  public:
    TerrainBuilderImpl(const TerrainConfig& config);
};

TerrainBuilderImpl::TerrainBuilderImpl(const TerrainConfig& config)
{
}

} // namespace

TerrainBuilderPtr createTerrainBuilder(const TerrainConfig& config)
{
  return std::make_unique<TerrainBuilderImpl>(config);
}

} // namespace lithic3d
