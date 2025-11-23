#pragma once

#include <memory>
#include <filesystem>

namespace lithic3d
{

class TerrainSystem
{
  public:
    virtual void loadHeightMap(const std::filesystem::path& path) = 0;

    virtual ~TerrainSystem() = default;
};

using TerrainSystemPtr = std::unique_ptr<TerrainSystem>;

TerrainSystemPtr createTerrainSystem();

} // namespace lithic3d
