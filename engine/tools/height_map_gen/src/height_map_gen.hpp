#pragma once

#include <filesystem>

namespace lithic3d
{
namespace tools
{

// Generate a height map (8-bit per pixel greyscale PNG image) from an ACE2 file from NASA's
// Altimeter Corrected Elevations, Version 2 (ACE2) dataset
void genHeightMap(const std::filesystem::path& inputFilePath,
  const std::filesystem::path& outputFilePath, float maxElevation);

} // namespace tools
} // namespace lithic3d
