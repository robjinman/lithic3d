#pragma once

#include <filesystem>

namespace lithic3d
{
namespace tools
{

void createWorld(const std::filesystem::path& heightMap, const std::filesystem::path& splatMap,
  uint32_t gridWidth, uint32_t gridHeight, float cellWidth, float cellHeight,
  const std::filesystem::path& rTexture, const std::filesystem::path& gTexture,
  const std::filesystem::path& bTexture, const std::filesystem::path& aTexture,
  float minElevation, float maxElevation, const std::filesystem::path& outputDir);

} // namespace tools
} // namespace lithic3d
