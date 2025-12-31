#pragma once

#include <filesystem>

void genHeightMap(const std::filesystem::path& inputFilePath,
  const std::filesystem::path& outputFilePath, float maxElevation);
