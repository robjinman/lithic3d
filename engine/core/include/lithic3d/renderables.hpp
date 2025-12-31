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

class AlignedBytes
{
  public:
    template<typename T>
    AlignedBytes(size_t numElements, T initialValue)
      : m_numElements(numElements)
      , m_elementSize(sizeof(T))
    {
      size_t n = sizeof(T) * numElements + alignof(T) - 1;
      m_data.resize(n);
      void* raw = m_data.data();
      m_alignedPtr = reinterpret_cast<char*>(std::align(alignof(T), sizeof(T), raw, n));

      new (m_alignedPtr) T[numElements];

      for (size_t i = 0; i < numElements; ++i) {
        std::memcpy(m_alignedPtr + i * sizeof(T), &initialValue, sizeof(T));
      }
    }

    template<typename T>
    explicit AlignedBytes(const std::vector<T>& data)
      : m_numElements(data.size())
      , m_elementSize(sizeof(T))
    {
      size_t n = sizeof(T) * data.size() + alignof(T) - 1;
      m_data.resize(n);
      void* raw = m_data.data();
      m_alignedPtr = reinterpret_cast<char*>(std::align(alignof(T), sizeof(T), raw, n));

      std::memcpy(m_alignedPtr, data.data(), data.size() * sizeof(T));
    }

    AlignedBytes(const AlignedBytes&) = delete;

    AlignedBytes(AlignedBytes&& mv)
      : m_numElements(mv.m_numElements)
      , m_elementSize(mv.m_elementSize)
      , m_data(std::move(mv.m_data))
      , m_alignedPtr(mv.m_alignedPtr)
    {}

    AlignedBytes& operator=(const AlignedBytes&) = delete;

    AlignedBytes& operator=(AlignedBytes&& rhs)
    {
      m_numElements = rhs.m_numElements;
      m_elementSize = rhs.m_elementSize;
      m_data = std::move(rhs.m_data);
      m_alignedPtr = rhs.m_alignedPtr;

      return *this;
    }

    char* rawBytes()
    {
      return m_alignedPtr;
    }

    const char* rawBytes() const
    {
      return m_alignedPtr;
    }

    char* elementPtr(size_t i)
    {
      return m_alignedPtr + i * m_elementSize;
    }

    const char* elementPtr(size_t i) const
    {
      return m_alignedPtr + i * m_elementSize;
    }

    template<typename T>
    T& element(size_t i)
    {
      return *reinterpret_cast<T*>(elementPtr(i));
    }

    template<typename T>
    const T& element(size_t i) const
    {
      return *reinterpret_cast<const T*>(elementPtr(i));
    }

    size_t sizeInBytes() const
    {
      return m_numElements * m_elementSize;
    }

    size_t numElements() const
    {
      return m_numElements;
    }

    size_t elementSize() const
    {
      return m_elementSize;
    }

    template<typename T>
    std::span<const T> data() const
    {
      return std::span<const T>(reinterpret_cast<const T*>(m_alignedPtr), m_numElements);
    }

    template<typename T>
    std::span<T> data()
    {
      return std::span<T>(reinterpret_cast<T*>(m_alignedPtr), m_numElements);
    }

  private:
    size_t m_numElements;
    size_t m_elementSize;
    std::vector<char> m_data;
    char* m_alignedPtr = nullptr;
};

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

class Buffer
{
  public:
    Buffer()
      : usage(BufferUsage::None)
      , data{1, 0}
    {}

    Buffer(AlignedBytes&& data, BufferUsage usage)
      : usage(usage)
      , data(std::move(data))
    {}

    Buffer(const Buffer&) = delete;

    Buffer(Buffer&& mv)
      : usage(mv.usage)
      , data(std::move(mv.data))
    {}

    Buffer& operator=(const Buffer&) = delete;

    Buffer& operator=(Buffer&& rhs)
    {
      usage = rhs.usage;
      data = std::move(rhs.data);

      return *this;
    }

    BufferUsage usage;
    AlignedBytes data;
};

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
  MaterialHandle material;
  Rectf textureSection;
};

TexturePtr loadRgbaTexture(const std::vector<char>& data);
TexturePtr loadGreyscaleTexture(const std::vector<char>& data);
MeshPtr cuboid(const Vec3f& size, const Vec2f& textureSize);
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
