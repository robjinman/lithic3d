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

void genHeightMap(const std::filesystem::path& inputFilePath,
  const std::filesystem::path& outputFilePath, float maxElevation)
{
  std::ifstream stream{inputFilePath, std::ios::binary};

  std::vector<char> buffer(18000 * 4);
  std::vector<float> data(buffer.size() / sizeof(float));
  std::vector<char> image(18000 * 18000);

  size_t totalBytesRead = 0;
  while (!stream.eof()) {
    stream.read(buffer.data(), buffer.size());

    size_t bytesRead = stream.gcount();
    size_t numFloatsRead = bytesRead / 4;

    if (bytesRead == 0) {
      assert(stream.eof());
      break;
    }

    std::memcpy(data.data(), buffer.data(), buffer.size());

    size_t n = totalBytesRead / 4;

    for (size_t i = 0; i < numFloatsRead; ++i) {
      float value = std::max(data[i], 0.f);

      image[n + i] = std::min(1.f, value / maxElevation) * 255.f;
    }

    totalBytesRead += bytesRead;
  }

  std::cout << "Total bytes read: " << totalBytesRead << std::endl;

  stbi_write_png(outputFilePath.c_str(), 18000, 18000, 1, image.data(), 18000);
}
