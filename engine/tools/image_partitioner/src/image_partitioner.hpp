#pragma once

#include <filesystem>

namespace lithic3d
{
namespace tools
{

enum class ImageType
{
  HeightMap,
  SplatMap
};

void partitionImage(const std::filesystem::path& filePath, ImageType type, uint32_t cellsX,
  uint32_t cellsY, const std::filesystem::path& outputDir);

} // namespace tools
} // namespace lithic3d
