#include "lithic3d/renderables.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/file_system.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fstream>
#include <cassert>
#include <set>

namespace lithic3d
{

std::ostream& operator<<(std::ostream& stream, const render::MeshFeatureSet& features)
{
  stream << features.vertexLayout << std::endl;
  stream << features.flags;

  return stream;
}

std::ostream& operator<<(std::ostream& stream, const render::MaterialFeatureSet& features)
{
  stream << features.flags;

  return stream;
}

namespace render
{
namespace
{

TexturePtr loadTexture(const std::vector<char>& data, int stbPixelLayout)
{
  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
    static_cast<int>(data.size()), &width, &height, &channels, stbPixelLayout);

  if (!pixels) {
    EXCEPTION("Failed to load texture image");
  }

  TexturePtr texture = std::make_unique<Texture>();
  texture->width = width;
  texture->height = height;
  texture->channels = channels;
  texture->data.resize(width * height * channels);
  memcpy(texture->data.data(), pixels, width * height * channels);

  stbi_image_free(pixels);

  return texture;
}

} // namespace

TexturePtr loadRgbaTexture(const std::vector<char>& data)
{
  return loadTexture(data, STBI_rgb_alpha);
}

TexturePtr loadGreyscaleTexture(const std::vector<char>& data)
{
  return loadTexture(data, STBI_grey);
}

MeshPtr cuboid(const Vec3f& size, const Vec2f& textureSize)
{
  float W = size[0];
  float H = size[1];
  float D = size[2];

  float w = W / 2.f;
  float h = H / 2.f;
  float d = D / 2.f;

  float u = textureSize[0];
  float v = textureSize[1];

  MeshPtr mesh = std::make_unique<Mesh>();
  mesh->featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrNormal,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };
  // Viewed from above
  //
  // A +---+ B
  //   |   |
  // D +---+ C
  //
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec3f>{
      // Bottom face
      { -w, -h, -d }, // A  0
      { w, -h, -d },  // B  1
      { w, -h, d },   // C  2
      { -w, -h, d },  // D  3

      // Top face
      { -w, h, d },   // D' 4
      { w, h, d },    // C' 5
      { w, h, -d },   // B' 6
      { -w, h, -d },  // A' 7

      // Right face
      { w, -h, d },   // C  8
      { w, -h, -d },  // B  9
      { w, h, -d },   // B' 10
      { w, h, d },    // C' 11

      // Left face
      { -w, -h, -d }, // A  12
      { -w, -h, d },  // D  13
      { -w, h, d },   // D' 14
      { -w, h, -d },  // A' 15

      // Far face
      { -w, -h, -d }, // A  16
      { -w, h, -d },  // A' 17
      { w, h, -d },   // B' 18
      { w, -h, -d },  // B  19

      // Near face
      { -w, -h, d },  // D  20
      { w, -h, d },   // C  21
      { w, h, d },    // C' 22
      { -w, h, d }    // D' 23
    }}, BufferUsage::AttrPosition
  });
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec3f>{
      // Bottom face
      { 0, -1, 0 },   // A  0
      { 0, -1, 0 },   // B  1
      { 0, -1, 0 },   // C  2
      { 0, -1, 0 },   // D  3

      // Top face
      { 0, 1, 0 },    // D' 4
      { 0, 1, 0 },    // C' 5
      { 0, 1, 0 },    // B' 6
      { 0, 1, 0 },    // A' 7

      // Right face
      { 1, 0, 0 },    // C  8
      { 1, 0, 0 },    // B  9
      { 1, 0, 0 },    // B' 10
      { 1, 0, 0 },    // C' 11

      // Left face
      { -1, 0, 0 },   // A  12
      { -1, 0, 0 },   // D  13
      { -1, 0, 0 },   // D' 14
      { -1, 0, 0 },   // A' 15

      // Far face
      { 0, 0, -1 },   // A  16
      { 0, 0, -1 },   // A' 17
      { 0, 0, -1 },   // B' 18
      { 0, 0, -1 },   // B  19

      // Near face
      { 0, 0, 1 },    // D  20
      { 0, 0, 1 },    // C  21
      { 0, 0, 1 },    // C' 22
      { 0, 0, 1 }     // D' 23
    }}, BufferUsage::AttrNormal
  });
  mesh->attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec2f>{
      // Bottom face
      { 0, 0 },         // A  0
      { W / u, 0 },     // B  1
      { W / u, D / v }, // C  2
      { 0, D / v },     // D  3

      // Top face
      { 0, D / v },     // D' 4
      { W / u, D / v }, // C' 5
      { W / u, 0 },     // B' 6
      { 0, 0 },         // A' 7

      // Right face
      { D / u, 0 },     // C  8
      { 0, 0 },         // B  9
      { 0, H / v },     // B' 10
      { D / u, H / v }, // C' 11

      // Left face
      { 0, 0 },         // A  12
      { D / u, 0 },     // D  13
      { D / u, H / v }, // D' 14
      { 0, H / v },     // A' 15

      // Far face
      { 0, 0 },         // A  16
      { 0, H / v },     // A' 17
      { W / u, H / v }, // B' 18
      { W / u, 0 },     // B  19

      // Near face
      { 0, 0 },         // D  20
      { W / u, 0 },     // C  21
      { W / u, H / v }, // C' 22
      { 0, H / v }      // D' 23
    }}, BufferUsage::AttrTexCoord
  });
  mesh->indexBuffer = Buffer{AlignedBytes{
    std::vector<uint16_t>{
      0, 1, 2, 0, 2, 3,         // Bottom face
      4, 5, 6, 4, 6, 7,         // Top face
      8, 9, 10, 8, 10, 11,      // Left face
      12, 13, 14, 12, 14, 15,   // Right face
      16, 17, 18, 16, 18, 19,   // Near face
      20, 21, 22, 20, 22, 23    // Far face
    }}, BufferUsage::Index
  };

  return mesh;
}

std::vector<char> createVertexArray(const Mesh& mesh)
{
  ASSERT(mesh.attributeBuffers.size() > 0, "Expected at least 1 attribute buffer");

  size_t numVertices = mesh.attributeBuffers[0].data.numElements();
  size_t vertexSize = 0;
  for (auto& buffer : mesh.attributeBuffers) {
    vertexSize += getAttributeSize(buffer.usage);

    ASSERT(buffer.data.numElements() == numVertices,
      "Expected all attribute buffers to have same length");
  }

  std::vector<char> array(numVertices * vertexSize);

  for (auto& buffer : mesh.attributeBuffers) {
    const char* srcPtr = buffer.data.rawBytes();
    char* destPtr = array.data();

    for (size_t i = 0; i < numVertices; ++i) {
      size_t offset = calcOffsetInVertex(mesh.featureSet.vertexLayout, buffer.usage);
      size_t attributeSize = getAttributeSize(buffer.usage);

      std::memcpy(destPtr + offset, srcPtr, attributeSize);

      srcPtr += attributeSize;
      destPtr += vertexSize;
    }
  }

  return array;
}

} // namespace render
} // namespace lithic3d
