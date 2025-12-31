#include "height_map_gen.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace tools
{

void genHeightMap(const std::filesystem::path& inputFilePath,
  const std::filesystem::path& outputFilePath, float maxElevation)
{
  const size_t tileWidth = 18000;
  const size_t tileHeight = 18000;

  std::ifstream stream{inputFilePath, std::ios::binary};

  std::vector<char> buffer(tileWidth * sizeof(float));
  std::vector<float> data(buffer.size() / sizeof(float));
  std::vector<char> image(tileWidth * tileHeight);

  size_t totalBytesRead = 0;
  while (!stream.eof()) {
    stream.read(buffer.data(), buffer.size());

    size_t bytesRead = stream.gcount();
    size_t numFloatsRead = bytesRead / sizeof(float);

    if (bytesRead == 0) {
      assert(stream.eof());
      break;
    }

    std::memcpy(data.data(), buffer.data(), buffer.size());

    size_t n = totalBytesRead / sizeof(float);

    for (size_t i = 0; i < numFloatsRead; ++i) {
      float value = std::max(data[i], 0.f);

      image[n + i] = std::min(1.f, value / maxElevation) * 255.f;
    }

    totalBytesRead += bytesRead;
  }

  // TODO: Logger
  std::cout << "Total bytes read: " << totalBytesRead << std::endl;

  stbi_write_png(outputFilePath.c_str(), tileWidth, tileHeight, 1, image.data(), tileWidth);
}

} // namespace tools
} // namespace lithic3d
