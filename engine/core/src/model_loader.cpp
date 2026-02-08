#include "lithic3d/model_loader.hpp"
#include "lithic3d/gltf.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/render_resource_loader.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/thread.hpp"
#include <set>

namespace lithic3d
{

using render::Buffer;
using render::BufferUsage;
using render::VertexLayout;
using render::Mesh;
using render::MaterialPtr;
namespace MeshFeatures = render::MeshFeatures;
using render::MeshFeatureSet;
using render::MeshHandle;
namespace MaterialFeatures = render::MaterialFeatures;
using render::MaterialFeatureSet;
using render::MaterialHandle;
using render::AlignedBytes;

namespace
{

AlignedBytes createAlignedBuffer(BufferUsage usage, const gltf::BufferDesc desc)
{
  switch (usage) {
    case BufferUsage::AttrPosition: return AlignedBytes{desc.size, Vec3f{}};
    case BufferUsage::AttrNormal: return AlignedBytes{desc.size, Vec3f{}};
    case BufferUsage::AttrTexCoord: return AlignedBytes{desc.size, Vec2f{}};
    case BufferUsage::AttrJointIndices: return AlignedBytes{desc.size, Vector<uint8_t, 4>{}};
    case BufferUsage::AttrJointWeights: return AlignedBytes{desc.size, Vector<float, 4>{}};
    // TODO
    default: EXCEPTION("Error creating aligned buffer");
  }
}

template<typename DEST_T, typename SRC_T>
DEST_T convertScalar(const char* bytes)
{
  SRC_T value = 0;
  std::memcpy(&value, bytes, sizeof(SRC_T));
  return static_cast<DEST_T>(value);
}

// Convert GLTF type to DEST_T
template<typename DEST_T>
DEST_T convertScalar(const char* bytes, gltf::ComponentType dataType)
{
  switch (dataType) {
    case gltf::ComponentType::SignedByte: {
      assert(gltf::getSize(dataType) == sizeof(int8_t));
      return convertScalar<DEST_T, int8_t>(bytes);
    }
    case gltf::ComponentType::UnsignedByte: {
      assert(gltf::getSize(dataType) == sizeof(uint8_t));
      return convertScalar<DEST_T, uint8_t>(bytes);
    }
    case gltf::ComponentType::SignedShort: {
      assert(gltf::getSize(dataType) == sizeof(int16_t));
      return convertScalar<DEST_T, int16_t>(bytes);
    }
    case gltf::ComponentType::UnsignedShort: {
      assert(gltf::getSize(dataType) == sizeof(uint16_t));
      return convertScalar<DEST_T, uint16_t>(bytes);
    }
    case gltf::ComponentType::UnsignedInt: {
      assert(gltf::getSize(dataType) == sizeof(uint32_t));
      return convertScalar<DEST_T, uint32_t>(bytes);
    }
    case gltf::ComponentType::Float: {
      assert(gltf::getSize(dataType) == sizeof(float));
      return convertScalar<DEST_T, float>(bytes);
    }
    default: EXCEPTION("Cannot convert data type");
  }
}

// DEST_T is scalar type, e.g. float
template<typename DEST_T, size_t ROWS, size_t COLS>
size_t convert(const char* src, gltf::ComponentType srcType, char* dest)
{
  auto componentSize = gltf::getSize(srcType);

  if (COLS == 1 && ROWS == 1) {     // Scalar
    *reinterpret_cast<DEST_T*>(dest) = convertScalar<DEST_T>(src, srcType);
    return sizeof(DEST_T);
  }
  else if (COLS == 1 && ROWS > 1) { // Vector
    auto& vector = *reinterpret_cast<Vector<DEST_T, ROWS>*>(dest);
    for (size_t i = 0; i < ROWS; ++i) {
      vector[i] = convertScalar<DEST_T>(src + i * componentSize, srcType);
    }
    return sizeof(Vector<DEST_T, ROWS>);
  }
  else if (COLS > 1) {              // Matrix
    auto& matrix = *reinterpret_cast<Matrix<DEST_T, ROWS, COLS>*>(dest);
    for (size_t c = 0; c < COLS; ++c) {
      for (size_t r = 0; r < ROWS; ++r) {
        matrix.set(r, c, convertScalar<DEST_T>(src + (c * ROWS + r) * componentSize, srcType));
      }
    }
    return sizeof(Matrix<DEST_T, ROWS, COLS>);
  }
  else {
    EXCEPTION("Unknown entity with " << ROWS << " rows and " << COLS << " columns");
  }
}

// Map gltf types to engine types
size_t convert(const char* src, gltf::ElementType elementType, gltf::ComponentType srcType,
  char* dest)
{
  switch (elementType) {
    case gltf::ElementType::AttrJointIndices:
      return convert<uint8_t, 4, 1>(src, srcType, dest);
    case gltf::ElementType::VertexIndex:
      return convert<uint16_t, 1, 1>(src, srcType, dest);
    case gltf::ElementType::AnimationTimestamps:
      return convert<float, 1, 1>(src, srcType, dest);
    case gltf::ElementType::AttrTexCoord:
      return convert<float, 2, 1>(src, srcType, dest);
    case gltf::ElementType::JointScale:
    case gltf::ElementType::JointTranslation:
    case gltf::ElementType::AttrPosition:
    case gltf::ElementType::AttrNormal:
      return convert<float, 3, 1>(src, srcType, dest);
    case gltf::ElementType::JointRotation:
    case gltf::ElementType::AttrJointWeights:
      return convert<float, 4, 1>(src, srcType, dest);
    case gltf::ElementType::JointInverseBindMatrices:
      return convert<float, 4, 4>(src, srcType, dest);
    default:
      EXCEPTION("Cannot convert element type");
  }
}

// Destination buffer should already have correct object types constructed there, so we can
// reinterpret_cast without causing UB.
void copyToBuffer(const std::vector<std::vector<char>>& srcBuffers, char* dstBuffer,
  const gltf::BufferDesc desc)
{
  const char* src = srcBuffers[desc.index].data() + desc.offset;

  size_t srcElemSize = gltf::getSize(desc.componentType) * desc.dimensions;
  DBG_ASSERT(srcElemSize * desc.size == desc.byteLength, "Buffer has unexpected length");

  char* dstPtr = dstBuffer;
  for (unsigned long i = 0; i < desc.size; ++i) {
    dstPtr += convert(src + i * srcElemSize, desc.type, desc.componentType, dstPtr);
  }
}

BufferUsage getUsage(gltf::ElementType type)
{
  switch (type) {
    case gltf::ElementType::AttrPosition: return BufferUsage::AttrPosition;
    case gltf::ElementType::AttrNormal: return BufferUsage::AttrNormal;
    case gltf::ElementType::AttrTexCoord: return BufferUsage::AttrTexCoord;
    case gltf::ElementType::AttrJointIndices: return BufferUsage::AttrJointIndices;
    case gltf::ElementType::AttrJointWeights: return BufferUsage::AttrJointWeights;
    // TODO
    default: EXCEPTION("Error converting ElementType to BufferUsage");
  }
}

VertexLayout getVertexLayout(const gltf::MeshDesc& meshDesc, bool hasTangents)
{
  VertexLayout layout{};

  size_t i = 0;
  for (auto& b : meshDesc.buffers) {
    if (gltf::isAttribute(b.type)) {
      layout[i++] = getUsage(b.type);
    }
  }
  if (hasTangents) {
    layout[i] = BufferUsage::AttrTangent;
  }

  std::sort(layout.begin(), layout.end(), [](auto a, auto b) {
    // Use arithmetic underflow to make 0 (BuffeUsage::None) the largest value
    return static_cast<uint8_t>(static_cast<uint8_t>(a) - 1) <
      static_cast<uint8_t>(static_cast<uint8_t>(b) - 1);
  });

  return layout;
}

void computeMeshTangents(Mesh& mesh)
{
  auto getBuffer = [](const std::vector<Buffer>& buffers, BufferUsage usage) -> const Buffer& {
    auto i = std::find_if(buffers.begin(), buffers.end(), [usage](const Buffer& buffer) {
      return buffer.usage == usage;
    });
    DBG_ASSERT(i != buffers.end(), "Mesh does not contain buffer of that type");
    return *i;
  };

  auto& posBuffer = getBuffer(mesh.attributeBuffers, BufferUsage::AttrPosition);
  auto& uvBuffer = getBuffer(mesh.attributeBuffers, BufferUsage::AttrTexCoord);

  auto positions = posBuffer.data.data<Vec3f>();
  auto texCoords = uvBuffer.data.data<Vec2f>();
  auto indices = mesh.indexBuffer.data.data<uint16_t>();

  DBG_ASSERT(positions.size() == texCoords.size(), "Expected equal number of positions and UVs");
  DBG_ASSERT(indices.size() % 3 == 0, "Expected indices buffer size to be multiple of 3");

  std::vector<Vec3f> tangents(positions.size());

  size_t n = indices.size();
  for (size_t i = 0; i < n; i += 3) {
    uint16_t aIdx = indices[i];
    uint16_t bIdx = indices[i + 1];
    uint16_t cIdx = indices[i + 2];

    auto& posA = positions[aIdx];
    auto& posB = positions[bIdx];
    auto& posC = positions[cIdx];

    auto& uvA = texCoords[aIdx];
    auto& uvB = texCoords[bIdx];
    auto& uvC = texCoords[cIdx];

    // TODO: Simplify. Don't need the bitangent

    Mat2x2f M = inverse(Mat2x2f{
      uvB[0] - uvA[0], uvC[0] - uvB[0],
      uvB[1] - uvA[1], uvC[1] - uvB[1]
    });

    Vec3f E = posB - posA;
    Vec3f F = posC - posB;

    Mat3x2f EF{
      E[0], F[0],
      E[1], F[1],
      E[2], F[2]
    };

    Mat3x2f TB = EF * M;

    Vec3f T{ TB.at(0, 0), TB.at(1, 0), TB.at(2, 0) };

    tangents[aIdx] += T;
    tangents[bIdx] += T;
    tangents[cIdx] += T;
  }

  mesh.attributeBuffers.push_back(Buffer{AlignedBytes{tangents}, BufferUsage::AttrTangent});
}

MeshFeatureSet createMeshFeatureSet(const gltf::MeshDesc& meshDesc)
{
  auto hasAttribute = [&](gltf::ElementType type) {
    for (auto& buf : meshDesc.buffers) {
      if (buf.type == type) {
        return true;
      }
    }
    return false;
  };

  bool hasTangents = !meshDesc.material.normalMap.empty();
  bool isAnimated = hasAttribute(gltf::ElementType::AttrJointIndices);

  MeshFeatures::Flags flags{};
  flags.set(MeshFeatures::CastsShadow, true);
  flags.set(MeshFeatures::HasTangents, hasTangents);
  flags.set(MeshFeatures::IsAnimated, isAnimated);

  MeshFeatureSet features{
    .vertexLayout = getVertexLayout(meshDesc, hasTangents),
    .flags = flags
  };

  return features;
}

MaterialFeatureSet createMaterialFeatureSet(const gltf::MaterialDesc& materialDesc)
{
  MaterialFeatureSet features;

  features.flags.set(MaterialFeatures::HasTexture, !materialDesc.baseColourTexture.empty());
  features.flags.set(MaterialFeatures::HasNormalMap, !materialDesc.normalMap.empty());
  features.flags.set(MaterialFeatures::IsDoubleSided, materialDesc.isDoubleSided);

  return features;
}

render::MeshPtr constructMesh(const gltf::MeshDesc& meshDesc,
  const std::vector<std::vector<char>>& dataBuffers)
{
  auto mesh = std::make_unique<Mesh>();
  mesh->featureSet = createMeshFeatureSet(meshDesc);
  mesh->transform = meshDesc.transform;

  // TODO: Set mesh rest pose

  std::set<gltf::ElementType> attributes;
  for (const auto& bufferDesc : meshDesc.buffers) {
    if (gltf::isAttribute(bufferDesc.type)) {
      DBG_ASSERT(!attributes.contains(bufferDesc.type),
        "Model contains multiple attribute buffers of same type");

      attributes.insert(bufferDesc.type);
    }
  }

  mesh->attributeBuffers.resize(attributes.size());

  for (const auto& bufferDesc : meshDesc.buffers) {
    if (bufferDesc.type == gltf::ElementType::VertexIndex) {
      mesh->indexBuffer = Buffer{AlignedBytes{bufferDesc.size, uint16_t{}}, BufferUsage::Index};
      copyToBuffer(dataBuffers, reinterpret_cast<char*>(mesh->indexBuffer.data.rawBytes()),
        bufferDesc);
    }
    else if (gltf::isAttribute(bufferDesc.type)) {
      auto usage = getUsage(bufferDesc.type);

      Buffer buffer{createAlignedBuffer(usage, bufferDesc), usage};
      copyToBuffer(dataBuffers, buffer.data.rawBytes(), bufferDesc);

      size_t index = std::distance(attributes.begin(), attributes.find(bufferDesc.type));
      mesh->attributeBuffers[index] = std::move(buffer);
    }
    else {
      // TODO
      EXCEPTION("Not implemented");
    }
  }

  return mesh;
}

class ModelLoaderImpl : public ModelLoader
{
  public:
    ModelLoaderImpl(RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
      const FileSystem& fileSystem, Logger& logger);

    ResourceHandle loadModelAsync(const std::filesystem::path& path) override;
    ResourceHandle loadModelAsync(ModelPtr model) override;

    const Model& getModel(ResourceId id) const override;

    ~ModelLoaderImpl() override;

  private:
    Logger& m_logger;
    RenderResourceLoader& m_renderResourceLoader;
    ResourceManager& m_resourceManager;
    const FileSystem& m_fileSystem;

    mutable std::mutex m_mutex;
    std::unordered_map<ResourceId, ModelPtr> m_models;

    MaterialHandle loadMaterialAsync(const gltf::MaterialDesc& desc);
    void loadMeshes(const std::vector<std::vector<char>>& dataBuffers,
      const gltf::ModelDesc& modelDesc, Model& model);
};

ModelLoaderImpl::ModelLoaderImpl(RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const FileSystem& fileSystem, Logger& logger)
  : m_logger(logger)
  , m_renderResourceLoader(renderResourceLoader)
  , m_resourceManager(resourceManager)
  , m_fileSystem(fileSystem)
{
}

const Model& ModelLoaderImpl::getModel(ResourceId id) const
{
  std::scoped_lock lock{m_mutex};
  return *m_models.at(id);
}

MaterialHandle ModelLoaderImpl::loadMaterialAsync(const gltf::MaterialDesc& desc)
{
  auto material = std::make_unique<render::Material>();
  material->name = desc.name;
  material->colour = desc.baseColourFactor;
  material->featureSet = createMaterialFeatureSet(desc);

  // TODO: Remove this. We need to provide material customisation
  if (material->name == "models/weapons/rocketlauncher/rocketlauncher_fx") {
    material->featureSet.flags.set(MaterialFeatures::HasTransparency);
  }

  std::filesystem::path texturesPath = "textures";

  if (!desc.baseColourTexture.empty()) {
    material->textures = {
      m_renderResourceLoader.loadTextureAsync(texturesPath / desc.baseColourTexture)
    };
  }

  if (!desc.normalMap.empty()) {
    material->normalMaps = {
      m_renderResourceLoader.loadNormalMapAsync(texturesPath / desc.normalMap)
    };
  }

  // TODO: Cube maps?

  return m_renderResourceLoader.loadMaterialAsync(std::move(material));
}

SkeletonPtr extractSkeleton(const gltf::ArmatureDesc& armature)
{
  auto skeleton = std::make_unique<Skeleton>();
  skeleton->rootNodeIndex = armature.rootNodeIndex;

  for (auto& node : armature.nodes) {
    skeleton->joints.push_back(Joint{
      .transform = node.transform,
      .children = node.children
    });
  }

  return skeleton;
}

SkinPtr constructSkin(const std::vector<std::vector<char>>& dataBuffers,
  const gltf::SkinDesc& skinDesc)
{
  auto skin = std::make_unique<Skin>();

  skin->inverseBindMatrices.resize(skinDesc.inverseBindMatricesBuffer.size);

  copyToBuffer(dataBuffers, reinterpret_cast<char*>(skin->inverseBindMatrices.data()),
    skinDesc.inverseBindMatricesBuffer);

  for (auto nodeNumber : skinDesc.nodeIndices) {
    skin->joints.push_back(nodeNumber);
  }

  return skin;
}

ResourceHandle ModelLoaderImpl::loadModelAsync(ModelPtr model)
{
  return m_resourceManager.loadResource([this, model = std::move(model)](ResourceId id) mutable {
    std::scoped_lock lock{m_mutex};
    m_models.insert({ id, std::move(model) });

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        std::scoped_lock lock{m_mutex};
        m_models.erase(id);
      }
    };
  });
}

std::vector<Transform> extractTransforms(const std::vector<std::vector<char>>& dataBuffers,
  const gltf::BufferDesc& transformBufferDesc)
{
  std::vector<Transform> transforms;

  switch (transformBufferDesc.type) {
    case gltf::ElementType::JointRotation: {
      std::vector<Vec4f> buffer(transformBufferDesc.size);
      copyToBuffer(dataBuffers, reinterpret_cast<char*>(buffer.data()), transformBufferDesc);

      for (size_t i = 0; i < buffer.size(); ++i) {
        Transform t;
        t.rotation = { buffer[i][3], buffer[i][0], buffer[i][1], buffer[i][2] };
        transforms.push_back(t);
      }

      break;
    }
    case gltf::ElementType::JointTranslation: {
      std::vector<Vec3f> buffer(transformBufferDesc.size);
      copyToBuffer(dataBuffers, reinterpret_cast<char*>(buffer.data()), transformBufferDesc);

      for (size_t i = 0; i < buffer.size(); ++i) {
        Transform t;
        t.translation = buffer[i];
        transforms.push_back(t);
      }

      break;
    }
    case gltf::ElementType::JointScale: {
      std::vector<Vec3f> buffer(transformBufferDesc.size);
      copyToBuffer(dataBuffers, reinterpret_cast<char*>(buffer.data()), transformBufferDesc);

      for (size_t i = 0; i < buffer.size(); ++i) {
        Transform t;
        t.scale = buffer[i];
        transforms.push_back(t);
      }

      break;
    }
    default:
      EXCEPTION("Unknown transform type");
  }

  return transforms;
}

void ModelLoaderImpl::loadMeshes(const std::vector<std::vector<char>>& dataBuffers,
  const gltf::ModelDesc& modelDesc, Model& model)
{
  bool hasAnimations = modelDesc.armature.animations.size() > 0;

  for (auto& meshDesc : modelDesc.meshes) {
    auto mesh = constructMesh(meshDesc, dataBuffers);
    auto meshFeatures = mesh->featureSet;

    if (meshFeatures.flags.test(MeshFeatures::HasTangents)) {
      computeMeshTangents(*mesh);
    }

    auto submodel = std::make_unique<Submodel>();
    submodel->mesh = m_renderResourceLoader.loadMeshAsync(std::move(mesh));
    submodel->material = loadMaterialAsync(meshDesc.material);

    if (hasAnimations) {
      submodel->skin = constructSkin(dataBuffers, meshDesc.skin);
    }

    model.submodels.push_back(std::move(submodel));
  }
}

void extractAnimations(const std::vector<std::vector<char>>& dataBuffers,
  const gltf::ModelDesc& modelDesc, Model& model)
{
  for (auto& animationDesc : modelDesc.armature.animations) {
    std::map<size_t, std::vector<float>> timestampBuffers;

    auto getTimestampBuffer = [&](size_t index) -> std::vector<float>& {
      auto i = timestampBuffers.find(index);
      if (i != timestampBuffers.end()) {
        return i->second;
      }
      auto& bufferDesc = animationDesc.buffers[index];
      DBG_ASSERT(bufferDesc.componentType == gltf::ComponentType::Float, "Expected float buffer");
      DBG_ASSERT(bufferDesc.dimensions == 1, "Expected scalar elements");
      std::vector<float> buffer(bufferDesc.size);
      copyToBuffer(dataBuffers, reinterpret_cast<char*>(buffer.data()), bufferDesc);
      timestampBuffers[index] = std::move(buffer);
      return timestampBuffers.at(index);
    };

    auto animation = std::make_unique<Animation>();
    animation->name = animationDesc.name;

    for (auto& channelDesc : animationDesc.channels) {
      auto& transformBufferDesc = animationDesc.buffers[channelDesc.transformsBufferIndex];

      animation->channels.push_back(AnimationChannel{
        .jointIndex = channelDesc.nodeIndex,
        .timestamps = getTimestampBuffer(channelDesc.timesBufferIndex),
        .transforms = extractTransforms(dataBuffers, transformBufferDesc)
      });
    }

    model.animations->animations[animation->name] = std::move(animation);
  }
}

ResourceHandle ModelLoaderImpl::loadModelAsync(const std::filesystem::path& filePath)
{
  return m_resourceManager.loadResource([this, filePath](ResourceId id) {
    auto modelDesc = gltf::extractModel(m_fileSystem.readAppDataFile(filePath));

    std::vector<std::vector<char>> dataBuffers;
    for (const auto& buffer : modelDesc.buffers) {
      auto binPath = std::filesystem::path{filePath}.parent_path() / buffer;
      dataBuffers.push_back(m_fileSystem.readAppDataFile(binPath));
    }

    bool hasAnimations = modelDesc.armature.animations.size() > 0;
    auto model = std::make_unique<Model>();

    if (hasAnimations) {
      model->animations = std::make_unique<AnimationSet>();
      model->animations->skeleton = extractSkeleton(modelDesc.armature);
    }

    loadMeshes(dataBuffers, modelDesc, *model);
    extractAnimations(dataBuffers, modelDesc, *model);

    {
      std::scoped_lock lock{m_mutex};
      m_models.insert({ id, std::move(model) });
    }

    return ManagedResource{
      .unloader = [this](ResourceId id) {
        std::scoped_lock lock{m_mutex};
        m_models.erase(id);
      }
    };
  });
}

ModelLoaderImpl::~ModelLoaderImpl()
{
  {
    std::scoped_lock lock{m_mutex};
    m_models.clear();
  }
  m_resourceManager.waitAll();
}

} // namespace

ModelLoaderPtr createModelLoader(RenderResourceLoader& renderResourceLoader,
  ResourceManager& resourceManager, const FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<ModelLoaderImpl>(renderResourceLoader, resourceManager, fileSystem,
    logger);
}

} // namespace lithic3d
