#include "image_partitioner.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <sstream>
#include <iostream> // TODO

namespace fs = std::filesystem;

void copyRegion(const unsigned char* src, int srcStride, int x, int y, int w, int h, int channels,
  unsigned char* dest)
{
  for (int row = 0; row < h; ++row) {
    int destOffset = w * channels * row;
    int srcOffset = (y + row) * srcStride + x * channels;
    std::memcpy(dest + destOffset, src + srcOffset, w * channels);
  }
}

void partitionHeightMap(const fs::path& filePath, uint32_t cellsX, uint32_t cellsY,
  const fs::path& outputDir)
{
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);

  if (channels != 1) {
    throw std::runtime_error("Expected height map to have 1 channel");
  }

  if (width % cellsX != 0) {
    throw std::runtime_error("Height map width must be divisible by number of cells");
  }

  if (height % cellsY != 0) {
    throw std::runtime_error("Height map height must be divisible by number of cells");
  }

  int cellW = width / cellsX;
  int cellH = height / cellsY;

  std::stringstream ss;

  for (int j = 0; j < cellsY; ++j) {
    for (int i = 0; i < cellsX; ++i) {
      int dx0 = i > 0 ? -1 : 0;
      int dx1 = i + 1 < cellsX ? 1 : 0;
      int dy0 = j > 0 ? -1 : 0;
      int dy1 = j + 1 < cellsY ? 1 : 0;

      int w = cellW - dx0 + dx1;
      int h = cellH - dy0 + dy1;

      std::vector<unsigned char> cellData;
      cellData.resize(w * h * channels);

      copyRegion(data, width * channels, i * cellW + dx0, j * cellH + dy0, w, h, channels,
        cellData.data());

      ss.str("");
      ss << std::setw(3) << std::setfill('0') << i << std::setw(3) << std::setfill('0') << j;

      fs::path outputPath = outputDir / ss.str() / "height_map.png";
      fs::create_directories(outputDir / ss.str());

      std::cout << "Writing file " << outputPath << std::endl;

      if (stbi_write_png(outputPath.c_str(), w, h, channels, cellData.data(), w * channels) == 0) {
        throw std::runtime_error("Writing png failed");
      }
    }
  }
}

void partitionSplatMap(const fs::path& filePath, uint32_t cellsX, uint32_t cellsY,
  const fs::path& outputDir)
{
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);

  if (channels != 4) {
    throw std::runtime_error("Expected splat map to have 4 channels");
  }

  if (width % cellsX != 0) {
    throw std::runtime_error("Splat map width must be divisible by number of cells");
  }

  if (height % cellsY != 0) {
    throw std::runtime_error("Splat map height must be divisible by number of cells");
  }

  int cellW = width / cellsX;
  int cellH = height / cellsY;

  std::stringstream ss;

  for (int j = 0; j < cellsY; ++j) {
    for (int i = 0; i < cellsX; ++i) {
      ss.str("");
      ss << std::setw(3) << std::setfill('0') << i << std::setw(3) << std::setfill('0') << j;

      fs::path outputPath = outputDir / ss.str() / "splat_map.png";
      fs::create_directories(outputDir / ss.str());

      int stride = cellW * channels;
      int offset = j * stride + i * channels;

      if (stbi_write_png(outputPath.c_str(), cellW, cellH, channels, data + offset, stride) == 0) {
        throw std::runtime_error("Writing png failed");
      }
    }
  }
}

void partitionImage(const fs::path& filePath, ImageType type, uint32_t cellsX, uint32_t cellsY,
  const fs::path& outputDir)
{
  switch (type) {
    case ImageType::HeightMap: return partitionHeightMap(filePath, cellsX, cellsY, outputDir);
    case ImageType::SplatMap: return partitionSplatMap(filePath, cellsX, cellsY, outputDir);
  }
}
