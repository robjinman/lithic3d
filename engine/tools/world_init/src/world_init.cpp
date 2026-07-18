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

void writeWorldXml(const fs::path& path, uint32_t gridW, uint32_t gridH, float cellW, float cellH,
  float minElevation, float maxElevation)
{
  std::ofstream stream{path};

  if (!stream.good()) {
    throw std::runtime_error{std::string{"Error opening file '"} + path.string() + "'"};
  }

  stream
    << "<world grid_width=\"" << gridW << "\" grid_height=\"" << gridH << "\" cell_width=\""
      << cellW << "\" cell_height=\"" << cellH << "\">"
    << "</world>";
}

void writeSliceZero(const fs::path& cellPath, uint32_t cellX, uint32_t cellY,
  const fs::path& rTexture, const fs::path& gTexture, const fs::path& bTexture,
  const fs::path& aTexture, float cellW, float cellH, float minElevation, float maxElevation,
  float waterLevel)
{
  std::ofstream stream{cellPath / "000.xml"};

  float h = maxElevation - minElevation;
  float x = cellW * cellX + cellW * 0.5f;
  float y = minElevation + 0.5f * h;
  float z = cellH * cellY + cellH * 0.5f;

  stream <<
    "<cell-slice>\n"
    "  <terrain water_level=\"" << waterLevel << "\">\n"
    "    <terrain_piece height_map=\"height_map.png\" inverted=\"false\">\n"
    "      <splat_map file=\"splat_map.png\">\n"
    "        <texture file=\"" << rTexture.string() << "\"/>\n"
    "        <texture file=\"" << gTexture.string() << "\"/>\n"
    "        <texture file=\"" << bTexture.string() << "\"/>\n"
    "        <texture file=\"" << aTexture.string() << "\"/>\n"
    "      </splat_map>\n"
    "      <pos x=\"" << x << "\" y=\"" << y << "\" z=\"" << z << "\"/>\n"
    "      <dim x=\"" << cellW << "\" y=\"" << h << "\" z=\"" << cellH << "\"/>\n"
    "    </terrain_piece>\n"
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

void createWorld(const fs::path& heightMap, const fs::path& splatMap, uint32_t gridW,
  uint32_t gridH, float cellW, float cellH, const fs::path& rTexture, const fs::path& gTexture,
  const fs::path& bTexture, const fs::path& aTexture, float minElevation, float maxElevation,
  float waterLevel, const fs::path& outputDir)
{
  writeWorldXml(outputDir / "world.xml", gridW, gridH, cellW, cellH, minElevation, maxElevation);

  partitionImage(heightMap, ImageType::HeightMap, gridW, gridH, outputDir);
  partitionImage(splatMap, ImageType::SplatMap, gridW, gridH, outputDir);

  for (uint32_t i = 0; i < gridW; ++i) {
    for (uint32_t j = 0; j < gridH; ++j) {
      fs::path cellPath = outputDir / cellDirectoryName(i, j);
      writeSliceZero(cellPath, i, j, rTexture, gTexture, bTexture, aTexture, cellW, cellH,
        minElevation, maxElevation, waterLevel);

      for (uint32_t slice = 1; slice < NUM_SLICES; ++slice) {
        writeSlice(cellPath, slice);
      }
    }
  }
}

} // namespace tools
} // namespace lithic3d
