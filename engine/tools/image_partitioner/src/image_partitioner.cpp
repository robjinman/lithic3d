#include "image_partitioner.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <iostream> // TODO

namespace fs = std::filesystem;

namespace lithic3d
{
namespace tools
{
namespace
{

// Copies cell (cellX, cellY) of size (cellW, cellH) into destination buffer. Cells overlap by 1
// pixel in each direction, so destination buffer should be of size (cellW + 1, cellH + 1).
void copyRegion(const unsigned char* src, int srcW, int srcH, int cellX, int cellY, int cellW,
  int cellH, int channels, unsigned char* dest)
{
  int srcStride = srcW * channels;
  int destStride = (cellW + 1) * channels;

  auto copyRow = [=](int srcRow, int destRow) {
    int srcX = cellX * cellW;
    int rowW = cellW;
    int destX = 1;

    if (srcX > 0) {
      --srcX;
      ++rowW;
      --destX;
    }

    std::memcpy(dest + destRow * destStride + destX * channels,
      src + srcRow * srcStride + srcX * channels, rowW * channels);
  };

  for (int row = -1; row < cellH; ++row) {
    int srcRow = cellY * cellH + row;
    if (srcRow > 0) {
      copyRow(srcRow, row + 1);
    }
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
      std::vector<unsigned char> cellData;
      cellData.resize((cellW + 1) * (cellH + 1) * channels);

      copyRegion(data, width, height, i, j, cellW, cellH, channels, cellData.data());

      ss.str("");
      ss << std::setw(3) << std::setfill('0') << i << std::setw(3) << std::setfill('0') << j;

      fs::path outputPath = outputDir / ss.str() / "height_map.png";
      fs::create_directories(outputDir / ss.str());

      std::cout << "Writing file " << outputPath << std::endl;

      if (stbi_write_png(outputPath.c_str(), cellW + 1, cellH + 1, channels, cellData.data(),
        (cellW + 1) * channels) == 0) {

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

} // namespace

void partitionImage(const fs::path& filePath, ImageType type, uint32_t cellsX, uint32_t cellsY,
  const fs::path& outputDir)
{
  switch (type) {
    case ImageType::HeightMap: return partitionHeightMap(filePath, cellsX, cellsY, outputDir);
    case ImageType::SplatMap: return partitionSplatMap(filePath, cellsX, cellsY, outputDir);
  }
}

} // namespace tools
} // namespace lithic3d
