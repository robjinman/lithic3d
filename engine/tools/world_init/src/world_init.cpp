#include "world_init.hpp"
#include "image_partitioner.hpp"
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace tools
{
namespace
{

const uint32_t NUM_SLICES = 6;

void writeWorldXml(const fs::path& path, uint32_t gridWidth, uint32_t gridHeight, float cellWidth,
  float cellHeight, float minElevation, float maxElevation)
{
  std::ofstream stream{path};

  if (!stream.good()) {
    throw std::runtime_error{std::string{"Error opening file '"} + path.string() + "'"};
  }

  stream
    << "<world grid-width=\"" << gridWidth << "\" grid-height=\"" << gridHeight
    << "\" cell-width=\"" << cellWidth << "\" cell-height=\"" << cellHeight
    << "\" min-elevation=\"" << minElevation << "\" max-elevation=\"" << maxElevation << "\">"
    << "</world>";
}

void writeSliceZero(const fs::path& cellPath, const fs::path& rTexture, const fs::path& gTexture,
  const fs::path& bTexture, const fs::path& aTexture)
{
  std::ofstream stream{cellPath / "000.xml"};

  stream <<
    "<cell-slice>\n"
    "  <terrain>\n"
    "    <splat-map>\n"
    "      <texture file=\"" << rTexture.string() << "\"/>\n"
    "      <texture file=\"" << gTexture.string() << "\"/>\n"
    "      <texture file=\"" << bTexture.string() << "\"/>\n"
    "      <texture file=\"" << aTexture.string() << "\"/>\n"
    "    </splat-map>\n"
    "  </terrain>\n"
    "</cell-slice>\n";
}

void writeSlice(const fs::path& cellPath, uint32_t sliceIdx)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << sliceIdx << ".xml";

  std::ofstream stream{cellPath / ss.str()};

  stream <<
    "<cell-slice>\n"
    "</cell-slice>\n";
}

std::string cellDirectoryName(uint32_t x, uint32_t y)
{
  std::stringstream ss;
  ss << std::setw(3) << std::setfill('0') << x << std::setw(3) << std::setfill('0') << y;
  return ss.str();
}

} // namespace

void createWorld(const fs::path& heightMap, const fs::path& splatMap, uint32_t gridWidth,
  uint32_t gridHeight, float cellWidth, float cellHeight, const fs::path& rTexture,
  const fs::path& gTexture, const fs::path& bTexture, const fs::path& aTexture,
  float minElevation, float maxElevation, const fs::path& outputDir)
{
  writeWorldXml(outputDir / "world.xml", gridWidth, gridHeight, cellWidth, cellHeight, minElevation,
    maxElevation);

  partitionImage(heightMap, ImageType::HeightMap, gridWidth, gridHeight, outputDir);
  partitionImage(splatMap, ImageType::SplatMap, gridWidth, gridHeight, outputDir);

  for (uint32_t i = 0; i < gridWidth; ++i) {
    for (uint32_t j = 0; j < gridHeight; ++j) {
      fs::path cellPath = outputDir / cellDirectoryName(i, j);
      writeSliceZero(cellPath, rTexture, gTexture, bTexture, aTexture);

      for (uint32_t slice = 1; slice < NUM_SLICES; ++slice) {
        writeSlice(cellPath, slice);
      }
    }
  }
}

} // namespace tools
} // namespace lithic3d
