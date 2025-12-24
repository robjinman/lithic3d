// This file contains types directly understood by the Renderer class along with some helper
// functions

#pragma once

#include "math.hpp"
#include "hash.hpp"
#include "resource_manager.hpp"
#include <memory>
#include <vector>
#include <string>
#include <span>
#include <optional>
#include <array>
#include <bitset>

namespace lithic3d
{
namespace render
{

template<typename T>
std::vector<char> toBytes(const std::vector<T>& data)
{
  const char* p = reinterpret_cast<const char*>(data.data());
  size_t n = data.size() * sizeof(T);
  return std::vector<char>(p, p + n);
}

template<typename T>
std::vector<T> fromBytes(const std::vector<char>& data)
{
  const T* p = reinterpret_cast<const T*>(data.data());
  DBG_ASSERT(data.size() % sizeof(T) == 0, "Cannot convert vector");
  size_t n = data.size() / sizeof(T);
  return std::vector<T>(p, p + n);
}

struct Texture
{
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 3;
  std::vector<uint8_t> data;
};

using TexturePtr = std::unique_ptr<Texture>;

enum class BufferUsage : uint8_t
{
  None = 0,
  AttrPosition = 1,
  AttrNormal,
  AttrTexCoord,
  AttrTangent,
  AttrJointIndices,
  AttrJointWeights,
  Index
};
const uint32_t LAST_ATTR_IDX = static_cast<uint32_t>(BufferUsage::AttrJointWeights);
const uint32_t MAX_ATTRIBUTES = LAST_ATTR_IDX;

inline size_t getAttributeSize(BufferUsage usage)
{
  switch (usage) {
    case BufferUsage::None:             return 0;
    case BufferUsage::AttrPosition:     return sizeof(Vec3f);
    case BufferUsage::AttrNormal:       return sizeof(Vec3f);
    case BufferUsage::AttrTexCoord:     return sizeof(Vec2f);
    case BufferUsage::AttrTangent:      return sizeof(Vec3f);
    case BufferUsage::AttrJointIndices: return sizeof(uint8_t) * 4;
    case BufferUsage::AttrJointWeights: return sizeof(float) * 4;
    case BufferUsage::Index:            return sizeof(uint16_t);
  }
  EXCEPTION("Error getting element size");
}

namespace MeshFeatures
{
enum Enum : uint32_t
{
  IsInstanced,
  IsTerrain,
  IsSkybox,
  IsAnimated,
  HasTangents,
  CastsShadow,
  IsQuad,
  IsDynamicText
};
using Flags = std::bitset<32>;
}

using VertexLayout = std::array<BufferUsage, MAX_ATTRIBUTES>;

struct MeshFeatureSet
{
  VertexLayout vertexLayout;
  MeshFeatures::Flags flags;

  bool operator==(const MeshFeatureSet& rhs) const = default;
};

namespace MaterialFeatures
{
enum Enum : uint32_t
{
  HasTransparency,
  HasTexture,
  HasNormalMap,
  HasCubeMap,
  IsDoubleSided
};
using Flags = std::bitset<32>;
}

struct MaterialFeatureSet
{
  MaterialFeatures::Flags flags;

  bool operator==(const MaterialFeatureSet& rhs) const = default;
};

struct Material
{
  std::string name;
  MaterialFeatureSet featureSet;
  Vec4f colour = { 1, 1, 1, 1 };
  std::vector<ResourceHandle> textures;
  std::vector<ResourceHandle> normalMaps;
  ResourceHandle cubeMap;
  float metallicFactor = 0.f;
  float roughnessFactor = 0.f;
};

using MaterialPtr = std::unique_ptr<Material>;

struct MaterialHandle
{
  ResourceHandle resource;
  render::MaterialFeatureSet features;

  const MaterialHandle& wait() const
  {
    resource.wait();
    return *this;
  }
};

// TODO: Rethink mesh buffers. Need to comply with strict aliasing and alignment rules.

struct Buffer
{
  BufferUsage usage;
  std::vector<char> data;

  size_t numElements() const
  {
    return data.size() / getAttributeSize(usage);
  }
};

template<typename T>
Buffer createBuffer(const std::vector<T>& data, BufferUsage usage)
{
  return Buffer{
    .usage = usage,
    .data = toBytes(data)
  };
}

inline size_t calcOffsetInVertex(const VertexLayout& layout, BufferUsage attribute)
{
  size_t sum = 0;
  for (auto attr : layout) {
    if (attr < attribute) {
      sum += getAttributeSize(attr);
    }
  }
  return sum;
}

struct Mesh
{
  Mat4x4f transform = identityMatrix<4>();
  MeshFeatureSet featureSet;
  std::vector<Buffer> attributeBuffers;
  Buffer indexBuffer;
  uint32_t maxInstances = 0;
};

using MeshPtr = std::unique_ptr<Mesh>;

struct MeshHandle
{
  ResourceHandle resource;
  render::MeshFeatureSet features;

  const MeshHandle& wait() const
  {
    resource.wait();
    return *this;
  }
};

struct BitmapFont
{
  ResourceHandle material;
  Rectf textureSection;
};

template<typename T>
std::span<const T> getConstBufferData(const Buffer& buffer)
{
  size_t attributeSize = getAttributeSize(buffer.usage);

  DBG_ASSERT(buffer.data.size() % attributeSize == 0, "Buffer has unexpected size");

  return std::span<const T>(
    reinterpret_cast<const T*>(buffer.data.data()),
    buffer.data.size() / attributeSize
  );
}

template<typename T>
std::span<T> getBufferData(Buffer& buffer)
{
  auto span = getConstBufferData<T>(buffer);
  return std::span<T>(const_cast<T*>(span.data()), span.size());
}

inline std::span<const uint16_t> getConstIndexBufferData(const Mesh& mesh)
{
  return std::span<const uint16_t>(
    reinterpret_cast<const uint16_t*>(mesh.indexBuffer.data.data()),
    mesh.indexBuffer.data.size() / sizeof(uint16_t)
  );
}

inline std::span<uint16_t> getIndexBufferData(Mesh& mesh)
{
  auto span = getConstIndexBufferData(mesh);
  return std::span<uint16_t>(const_cast<uint16_t*>(span.data()), span.size());
}

TexturePtr loadRgbaTexture(const std::vector<char>& data);
TexturePtr loadGreyscaleTexture(const std::vector<char>& data);
MeshPtr cuboid(const Vec3f& size, const Vec2f& textureSize);
MeshPtr mergeMeshes(const Mesh& A, const Mesh& B);
std::vector<char> createVertexArray(const Mesh& mesh);

} // namespace render

inline std::ostream& operator<<(std::ostream& stream, const render::BufferUsage& usage)
{
  return stream << static_cast<int>(usage);
}

std::ostream& operator<<(std::ostream& stream, const render::MeshFeatureSet& features);
std::ostream& operator<<(std::ostream& stream, const render::MaterialFeatureSet& features);

} // namespace lithic3d

template<>
struct std::hash<lithic3d::render::MeshFeatureSet>
{
  size_t operator()(const lithic3d::render::MeshFeatureSet& x) const noexcept
  {
    return lithic3d::hashAll(
      x.vertexLayout,
      x.flags
    );
  }
};

template<>
struct std::hash<lithic3d::render::MaterialFeatureSet>
{
  size_t operator()(const lithic3d::render::MaterialFeatureSet& x) const noexcept
  {
    return lithic3d::hashAll(
      x.flags
    );
  }
};
