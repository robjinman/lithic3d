#include "vulkan/render_resources.hpp"
#include "vulkan/gpu_buffer_manager.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/trace.hpp"
#include "lithic3d/utils.hpp"
#include <map>
#include <array>
#include <cstring>
#include <cassert>

template<typename T, size_t N>
std::array<T, N> fillArray(const std::function<T()>& fn)
{
  std::array<T, N> arr;
  for (size_t i = 0; i < N; ++i) {
    arr[i] = fn();
  }
  return arr;
}

namespace lithic3d
{
namespace render
{
namespace
{

struct MeshData
{
  MeshPtr mesh = nullptr;
  GpuBufferPtr vertexBuffer;
  GpuBufferPtr indexBuffer;
  GpuBufferPtr instanceBuffer;
  uint32_t numInstances = 0;
  std::vector<VkDescriptorSet> objectDescriptorSets;
  std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> jointTransformsUbo;
};

using MeshDataPtr = std::unique_ptr<MeshData>;

struct TextureData
{
  TexturePtr texture;
  GpuImagePtr image;
};

using TextureDataPtr = std::unique_ptr<TextureData>;

struct CubeMapData
{
  std::array<TexturePtr, 6> textures;
  GpuImagePtr image;
};

using CubeMapDataPtr = std::unique_ptr<CubeMapData>;

struct MaterialData
{
  MaterialPtr material;
  VkDescriptorSet descriptorSet;
  GpuBufferPtr ubo;
};

using MaterialDataPtr = std::unique_ptr<MaterialData>;

enum class GlobalDescriptorSetBindings : uint32_t
{
  LightTransformsUbo = 0
};

enum class RenderPassDescriptorSetBindings : uint32_t
{
  CameraTransformsUbo = 0,
  LightingUbo = 1,
  ShadowMap = 2
};

enum class MaterialDescriptorSetBindings : uint32_t
{
  Ubo = 0,
  TextureSampler = 1,
  NormapMapSampler = 2,
  CubeMapSampler = 3
};

enum class ObjectDescriptorSetBindings : uint32_t
{
  JointTransformsUbo = 0
};

class RenderResourcesImpl : public RenderResources
{
  public:
    RenderResourcesImpl(GpuBufferManager& bufferManager, VkPhysicalDevice physicalDevice,
      VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, Logger& logger);

    // Descriptor sets
    //
    VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetNumber number) const override;
    VkDescriptorSet getGlobalDescriptorSet(size_t currentFrame) const override;
    VkDescriptorSet getRenderPassDescriptorSet(RenderPass renderPass,
      size_t currentFrame) const override;
    VkDescriptorSet getMaterialDescriptorSet(RenderItemId id) const override;
    VkDescriptorSet getObjectDescriptorSet(RenderItemId id, size_t currentFrame) const override;

    // Resources
    //
    RenderItemId addTexture(TexturePtr texture) override;
    RenderItemId addNormalMap(TexturePtr texture) override;
    RenderItemId addCubeMap(std::array<TexturePtr, 6> textures) override;
    void removeTexture(RenderItemId id) override;
    void removeCubeMap(RenderItemId id) override;

    // Meshes
    //
    MeshHandle addMesh(MeshPtr mesh) override;
    void removeMesh(RenderItemId id) override;
    void updateJointTransforms(RenderItemId meshId, const std::vector<Mat4x4f>& joints,
      size_t currentFrame) override;
    MeshBuffers getMeshBuffers(RenderItemId id) const override;
    void updateMeshInstances(RenderItemId id, const std::vector<MeshInstance>& instances) override;
    const MeshFeatureSet& getMeshFeatures(RenderItemId id) const override;

    // Materials
    //
    MaterialHandle addMaterial(MaterialPtr material) override;
    void removeMaterial(RenderItemId id) override;
    const MaterialFeatureSet& getMaterialFeatures(RenderItemId id) const override;

    // Transforms
    //
    // > Camera
    void updateMainCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) override;
    void updateOverlayCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) override;
    // > Light
    void updateLightTransformsUbo(const LightTransformsUbo& ubo, size_t currentFrame) override;

    // Lighting
    //
    void updateLightingUbo(const LightingUbo& ubo, size_t currentFrame) override;

    // Shadow pass
    //
    GpuImage& getShadowMap() override;

    ~RenderResourcesImpl() override;

  private:
    std::map<RenderItemId, MeshDataPtr> m_meshes;
    std::map<RenderItemId, TextureDataPtr> m_textures;
    std::map<RenderItemId, CubeMapDataPtr> m_cubeMaps;
    std::map<RenderItemId, MaterialDataPtr> m_materials;

    Logger& m_logger;
    GpuBufferManager& m_bufferManager;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkCommandPool m_commandPool;
    VkDescriptorPool m_descriptorPool;

    VkDescriptorSetLayout m_globalDescriptorSetLayout;
    VkDescriptorSetLayout m_renderPassDescriptorSetLayout;
    VkDescriptorSetLayout m_materialDescriptorSetLayout;
    VkDescriptorSetLayout m_objectDescriptorSetLayout;

    std::vector<VkDescriptorSet> m_globalDescriptorSets;
    std::vector<VkDescriptorSet> m_mainPassDescriptorSets;
    std::vector<VkDescriptorSet> m_overlayPassDescriptorSets;
    //std::vector<VkDescriptorSet> m_shadowPassDescriptorSets;

    std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_mainCameraUbo;
    std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_overlayCameraUbo;
    std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_lightTransformsUbo;
    std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_lightingUbo;

    VkSampler m_textureSampler;
    VkSampler m_normalMapSampler;
    VkSampler m_cubeMapSampler;

    VkExtent2D m_shadowMapSize{ SHADOW_MAP_W, SHADOW_MAP_H };
    GpuImagePtr m_shadowMapImage;
    VkSampler m_shadowMapSampler;

    RenderItemId m_nextTextureId = 1;
    RenderItemId m_nextCubeMapId = 1;

    void createTextureSampler();
    void createNormalMapSampler();
    void createCubeMapSampler();
    void createDescriptorPool();
    void addSamplerToDescriptorSet(VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler,
      uint32_t binding);

    void createUbos();
    void createShadowMap();

    void createGlobalDescriptorSetLayout();
    void createRenderPassDescriptorSetLayout();
    void createMaterialDescriptorSetLayout();
    void createObjectDescriptorSetLayout();

    void createGlobalDescriptorSet();
    void createMainPassDescriptorSet();
    void createOverlayPassDescriptorSet();
    //void createShadowPassDescriptorSet();
};

RenderResourcesImpl::RenderResourcesImpl(GpuBufferManager& bufferManager,
  VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue,
  VkCommandPool commandPool, Logger& logger)
  : m_logger(logger)
  , m_bufferManager(bufferManager)
  , m_physicalDevice(physicalDevice)
  , m_device(device)
  , m_graphicsQueue(graphicsQueue)
  , m_commandPool(commandPool)
{
  DBG_TRACE(m_logger);

  createUbos();
  createShadowMap();
  createDescriptorPool();
  createMaterialDescriptorSetLayout();
  createGlobalDescriptorSet();
  createRenderPassDescriptorSetLayout();
  createMainPassDescriptorSet();
  createOverlayPassDescriptorSet();
  //createShadowPassDescriptorSet();
  createObjectDescriptorSetLayout();
}

RenderItemId RenderResourcesImpl::addTexture(TexturePtr texture)
{
  auto textureData = std::make_unique<TextureData>();
  textureData->image = m_bufferManager.createTexture(*texture);
  textureData->texture = std::move(texture);

  auto textureId = m_nextTextureId++;
  m_textures[textureId] = std::move(textureData);

  return textureId;
}

RenderItemId RenderResourcesImpl::addNormalMap(TexturePtr texture)
{
  auto textureData = std::make_unique<TextureData>();
  textureData->image = m_bufferManager.createNormalMap(*texture);
  textureData->texture = std::move(texture);

  auto textureId = m_nextTextureId++;
  m_textures[textureId] = std::move(textureData);

  return textureId;
}

RenderItemId RenderResourcesImpl::addCubeMap(std::array<TexturePtr, 6> textures)
{
  auto cubeMapData = std::make_unique<CubeMapData>();
  cubeMapData->image = m_bufferManager.createCubeMap(textures);
  cubeMapData->textures = std::move(textures);

  auto cubeMapId = m_nextCubeMapId++;
  m_cubeMaps[cubeMapId] = std::move(cubeMapData);

  return cubeMapId;
}

void RenderResourcesImpl::removeTexture(RenderItemId id)
{
  auto i = m_textures.find(id);
  if (i == m_textures.end()) {
    return;
  }

  m_textures.erase(i);
}

void RenderResourcesImpl::removeCubeMap(RenderItemId id)
{
  auto i = m_cubeMaps.find(id);
  if (i == m_cubeMaps.end()) {
    return;
  }

  m_cubeMaps.erase(i);
}

MeshHandle RenderResourcesImpl::addMesh(MeshPtr mesh)
{
  static RenderItemId nextMeshId = 1;

  MeshHandle handle;
  handle.transform = mesh->transform;
  handle.features = mesh->featureSet;

  auto data = std::make_unique<MeshData>();
  data->mesh = std::move(mesh);
  data->vertexBuffer = m_bufferManager.createVertexBuffer(createVertexArray(*data->mesh));
  data->indexBuffer = m_bufferManager.createIndexBuffer(data->mesh->indexBuffer.data);
  if (data->mesh->featureSet.flags.test(MeshFeatures::IsInstanced)) {
    data->instanceBuffer =
      m_bufferManager.createInstanceBuffer(data->mesh->maxInstances * sizeof(MeshInstance));
  }

  if (data->mesh->featureSet.flags.test(MeshFeatures::IsAnimated)) {
    data->jointTransformsUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
      return m_bufferManager.createUbo(sizeof(JointTransformsUbo));
    });

    std::vector<Mat4x4f> joints(MAX_JOINTS, identityMatrix<4>());
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_bufferManager.writeToBuffer(*data->jointTransformsUbo[i], joints.data(),
        joints.size() * sizeof(Mat4x4f));
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_objectDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = m_descriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data()
    };

    data->objectDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, data->objectDescriptorSets.data()),
      "Failed to allocate descriptor sets");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VkDescriptorBufferInfo bufferInfo{
        .buffer = data->jointTransformsUbo[i]->vkBuffer(),
        .offset = 0,
        .range = sizeof(JointTransformsUbo)
      };

      VkWriteDescriptorSet descriptorWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = data->objectDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(ObjectDescriptorSetBindings::JointTransformsUbo),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr
      };

      vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }
  }

  handle.id = nextMeshId++;
  m_meshes[handle.id] = std::move(data);

  return handle;
}

void RenderResourcesImpl::removeMesh(RenderItemId id)
{
  auto i = m_meshes.find(id);
  if (i == m_meshes.end()) {
    return;
  }
  m_meshes.erase(i);
}

MeshBuffers RenderResourcesImpl::getMeshBuffers(RenderItemId id) const
{
  auto& mesh = m_meshes.at(id);

  return {
    .vertexBuffer = mesh->vertexBuffer != nullptr ?
      mesh->vertexBuffer->vkBuffer() : VK_NULL_HANDLE,
    .indexBuffer = mesh->indexBuffer!= nullptr ?
      mesh->indexBuffer->vkBuffer() : VK_NULL_HANDLE,
    .instanceBuffer = mesh->instanceBuffer != nullptr ?
      mesh->instanceBuffer->vkBuffer() : VK_NULL_HANDLE,
    .numIndices = static_cast<uint32_t>(mesh->mesh->indexBuffer.data.size() / sizeof(uint16_t)),
    .numInstances = mesh->numInstances
  };
}

// TODO: This is far too slow
void RenderResourcesImpl::updateMeshInstances(RenderItemId id,
  const std::vector<MeshInstance>& instances)
{
  DBG_TRACE(m_logger);

  auto& mesh = m_meshes.at(id);
  ASSERT(mesh->mesh->featureSet.flags.test(MeshFeatures::IsInstanced),
    "Can't instance a non-instanced mesh");
  ASSERT(instances.size() <= mesh->mesh->maxInstances, "Max instances exceeded for this mesh");

  mesh->numInstances = static_cast<uint32_t>(instances.size());
  m_bufferManager.writeToBuffer(*mesh->instanceBuffer, instances.data(), instances.size());
}

void RenderResourcesImpl::updateJointTransforms(RenderItemId id, const std::vector<Mat4x4f>& joints,
  size_t currentFrame)
{
  DBG_ASSERT(joints.size() <= MAX_JOINTS, "Max number of joints exceeded");

  auto& mesh = *m_meshes.at(id);
  m_bufferManager.writeToBuffer(*mesh.jointTransformsUbo[currentFrame], joints.data(),
    joints.size() * sizeof(Mat4x4f));
}

const MeshFeatureSet& RenderResourcesImpl::getMeshFeatures(RenderItemId id) const
{
  return m_meshes.at(id)->mesh->featureSet;
}

void RenderResourcesImpl::addSamplerToDescriptorSet(VkDescriptorSet descriptorSet,
  VkImageView imageView, VkSampler sampler, uint32_t binding)
{
  VkDescriptorImageInfo imageInfo{
    .sampler = sampler,
    .imageView = imageView,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  };

  VkWriteDescriptorSet samplerDescriptorWrite{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = descriptorSet,
    .dstBinding = binding,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .pImageInfo = &imageInfo,
    .pBufferInfo = nullptr,
    .pTexelBufferView = nullptr
  };

  vkUpdateDescriptorSets(m_device, 1, &samplerDescriptorWrite, 0, nullptr);
}

MaterialHandle RenderResourcesImpl::addMaterial(MaterialPtr material)
{
  static RenderItemId nextMaterialId = 1;

  MaterialHandle handle;
  handle.features = material->featureSet;

  auto materialData = std::make_unique<MaterialData>();

  VkDescriptorSetAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = m_descriptorPool,
    .descriptorSetCount = 1,
    .pSetLayouts = &m_materialDescriptorSetLayout
  };

  VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &materialData->descriptorSet),
    "Failed to allocate descriptor sets");

  materialData->ubo = m_bufferManager.createUbo(sizeof(MaterialUbo));

  VkDescriptorBufferInfo bufferInfo{
    .buffer = materialData->ubo->vkBuffer(),
    .offset = 0,
    .range = sizeof(MaterialUbo)
  };

  VkWriteDescriptorSet uboDescriptorWrite{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = nullptr,
    .dstSet = materialData->descriptorSet,
    .dstBinding = static_cast<uint32_t>(MaterialDescriptorSetBindings::Ubo),
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pImageInfo = nullptr,
    .pBufferInfo = &bufferInfo,
    .pTexelBufferView = nullptr
  };

  vkUpdateDescriptorSets(m_device, 1, &uboDescriptorWrite, 0, nullptr);

  // TODO: Use staging buffer instead of host mapping
  MaterialUbo ubo{
    .colour = material->colour
    // TODO: PBR properties
  };

  m_bufferManager.writeToBuffer(*materialData->ubo, &ubo, sizeof(ubo));

  // TODO: Use array of descriptors for textures, normal maps, etc.?
  if (material->featureSet.flags.test(MaterialFeatures::HasTexture)) {
    VkImageView imageView = m_textures.at(material->texture.id)->image->vkImageView();
    addSamplerToDescriptorSet(materialData->descriptorSet, imageView, m_textureSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::TextureSampler));
  }
  if (material->featureSet.flags.test(MaterialFeatures::HasNormalMap)) {
    VkImageView imageView = m_textures.at(material->normalMap.id)->image->vkImageView();
    addSamplerToDescriptorSet(materialData->descriptorSet, imageView, m_normalMapSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::NormapMapSampler));
  }
  if (material->featureSet.flags.test(MaterialFeatures::HasCubeMap)) {
    VkImageView imageView = m_cubeMaps.at(material->cubeMap.id)->image->vkImageView();
    addSamplerToDescriptorSet(materialData->descriptorSet, imageView, m_cubeMapSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::CubeMapSampler));
  }

  handle.id = nextMaterialId++;
  materialData->material = std::move(material);
  m_materials[handle.id] = std::move(materialData);

  return handle;
}

void RenderResourcesImpl::removeMaterial(RenderItemId id)
{
  auto i = m_materials.find(id);
  if (i == m_materials.end()) {
    return;
  }

  m_materials.erase(i);
}

VkDescriptorSet RenderResourcesImpl::getMaterialDescriptorSet(RenderItemId id) const
{
  return id == NULL_ID ? VK_NULL_HANDLE : m_materials.at(id)->descriptorSet;
}

VkDescriptorSet RenderResourcesImpl::getObjectDescriptorSet(RenderItemId id,
  size_t currentFrame) const
{
  // TODO: Currently assume object is a mesh
  auto& mesh = *m_meshes.at(id);
  return mesh.objectDescriptorSets.size() > 0 ?
    mesh.objectDescriptorSets[currentFrame] :
    VK_NULL_HANDLE;
}

const MaterialFeatureSet& RenderResourcesImpl::getMaterialFeatures(RenderItemId id) const
{
  return m_materials.at(id)->material->featureSet;
}

void RenderResourcesImpl::updateMainCameraUbo(const CameraTransformsUbo& ubo,
  size_t currentFrame)
{
  m_bufferManager.writeToBuffer(*m_mainCameraUbo[currentFrame], &ubo, sizeof(ubo));
}

void RenderResourcesImpl::updateOverlayCameraUbo(const CameraTransformsUbo& ubo,
  size_t currentFrame)
{
  m_bufferManager.writeToBuffer(*m_overlayCameraUbo[currentFrame], &ubo, sizeof(ubo));
}

void RenderResourcesImpl::updateLightTransformsUbo(const LightTransformsUbo& ubo,
  size_t currentFrame)
{
  m_bufferManager.writeToBuffer(*m_lightTransformsUbo[currentFrame], &ubo, sizeof(ubo));
}

VkDescriptorSetLayout RenderResourcesImpl::getDescriptorSetLayout(DescriptorSetNumber number) const
{
  switch (number) {
    case DescriptorSetNumber::Global: return m_globalDescriptorSetLayout;
    case DescriptorSetNumber::RenderPass: return m_renderPassDescriptorSetLayout;
    case DescriptorSetNumber::Material: return m_materialDescriptorSetLayout;
    case DescriptorSetNumber::Object: return m_objectDescriptorSetLayout;
  };
  EXCEPTION("Unknown descriptor set");
}

VkDescriptorSet RenderResourcesImpl::getGlobalDescriptorSet(size_t currentFrame) const
{
  return m_globalDescriptorSets[currentFrame];
}

void RenderResourcesImpl::updateLightingUbo(const LightingUbo& ubo, size_t currentFrame)
{
  m_bufferManager.writeToBuffer(*m_lightingUbo[currentFrame], &ubo, sizeof(ubo));
}

VkDescriptorSet RenderResourcesImpl::getRenderPassDescriptorSet(RenderPass renderPass,
  size_t currentFrame) const
{
  switch (renderPass) {
    case RenderPass::Main: return m_mainPassDescriptorSets[currentFrame];
    case RenderPass::Shadow: return m_mainPassDescriptorSets[currentFrame]; // TODO
    case RenderPass::Ssr: return m_mainPassDescriptorSets[currentFrame];    // TODO
    case RenderPass::Overlay: return m_overlayPassDescriptorSets[currentFrame];
  }
  EXCEPTION("Unknown render pass");
}

GpuImage& RenderResourcesImpl::getShadowMap()
{
  return *m_shadowMapImage;
}

void RenderResourcesImpl::createUbos()
{
  m_mainCameraUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
    return m_bufferManager.createUbo(sizeof(CameraTransformsUbo));
  });
  m_overlayCameraUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
    return m_bufferManager.createUbo(sizeof(CameraTransformsUbo));
  });
  m_lightTransformsUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
    return m_bufferManager.createUbo(sizeof(LightTransformsUbo));
  });
  m_lightingUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
    return m_bufferManager.createUbo(sizeof(LightingUbo));
  });
}

void RenderResourcesImpl::createShadowMap()
{
  m_shadowMapImage = m_bufferManager.createShadowMap(m_shadowMapSize);

  VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .mipLodBias = 0.f,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 1.f,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_NEVER,
    .minLod = 0.f,
    .maxLod = 1.f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_shadowMapSampler),
    "Failed to create shadow map sampler");
}

void RenderResourcesImpl::createDescriptorPool()
{
  DBG_TRACE(m_logger);

  std::array<VkDescriptorPoolSize, 2> poolSizes{};

  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 100; // TODO

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 100; // TODO

  VkDescriptorPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .maxSets = 200, // TODO
    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes = poolSizes.data()
  };

  VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool),
    "Failed to create descriptor pool");
}

void RenderResourcesImpl::createGlobalDescriptorSetLayout()
{
  DBG_TRACE(m_logger);

  VkDescriptorSetLayoutBinding lightLayoutBinding{
    .binding = static_cast<uint32_t>(GlobalDescriptorSetBindings::LightTransformsUbo),
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .pImmutableSamplers = nullptr
  };

  std::vector<VkDescriptorSetLayoutBinding> bindings{
    lightLayoutBinding
    // ...
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data()
  };

  VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
    &m_globalDescriptorSetLayout), "Failed to create descriptor set layout");

  m_logger.info(STR("Global descriptor set layout: " << m_globalDescriptorSetLayout));
}

void RenderResourcesImpl::createRenderPassDescriptorSetLayout()
{
  DBG_TRACE(m_logger);

  VkDescriptorSetLayoutBinding cameraLayoutBinding{
    .binding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::CameraTransformsUbo),
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding lightingUboLayoutBinding{
    .binding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::LightingUbo),
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding shadowMapLayoutBinding{
    .binding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::ShadowMap),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  std::vector<VkDescriptorSetLayoutBinding> bindings{
    cameraLayoutBinding,
    lightingUboLayoutBinding,
    shadowMapLayoutBinding
    // ...
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data()
  };
  
  VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
    &m_renderPassDescriptorSetLayout), "Failed to create descriptor set layout");

  m_logger.info(STR("Render pass descriptor set layout: " << m_renderPassDescriptorSetLayout));
}

void RenderResourcesImpl::createMaterialDescriptorSetLayout()
{
  DBG_TRACE(m_logger);

  createTextureSampler();
  createNormalMapSampler();
  createCubeMapSampler();

  VkDescriptorSetLayoutBinding uboLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::Ubo),
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding textureSamplerLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::TextureSampler),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding normalMapLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::NormapMapSampler),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding cubeMapLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::CubeMapSampler),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  std::vector<VkDescriptorSetLayoutBinding> bindings{
    textureSamplerLayoutBinding,
    normalMapLayoutBinding,
    cubeMapLayoutBinding,
    uboLayoutBinding
    // ...
  };

  // TODO: Remove this?
  std::array<VkDescriptorBindingFlags, 4> bindingFlags = {
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
  };

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .pNext = nullptr,
    .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
    .pBindingFlags = bindingFlags.data()
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,//&bindingFlagsInfo,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data()
  };

  VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
    &m_materialDescriptorSetLayout), "Failed to create descriptor set layout");

  m_logger.info(STR("Material descriptor set layout: " << m_materialDescriptorSetLayout));
}

void RenderResourcesImpl::createObjectDescriptorSetLayout()
{
  DBG_TRACE(m_logger);

  VkDescriptorSetLayoutBinding jointTransformsUboLayoutBinding{
    .binding = static_cast<uint32_t>(ObjectDescriptorSetBindings::JointTransformsUbo),
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .pImmutableSamplers = nullptr
  };

  std::vector<VkDescriptorSetLayoutBinding> bindings{
    jointTransformsUboLayoutBinding,
    // ...
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data()
  };
  
  VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
    &m_objectDescriptorSetLayout), "Failed to create descriptor set layout");

  m_logger.info(STR("Object descriptor set layout: " << m_objectDescriptorSetLayout));
}

void RenderResourcesImpl::createGlobalDescriptorSet()
{
  createGlobalDescriptorSetLayout();

  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_globalDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = m_descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
    .pSetLayouts = layouts.data()
  };

  m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_globalDescriptorSets.data()),
    "Failed to allocate descriptor sets");

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorBufferInfo lightBufferInfo{
      .buffer = m_lightTransformsUbo[i]->vkBuffer(),
      .offset = 0,
      .range = sizeof(LightTransformsUbo)
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites{
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_globalDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(GlobalDescriptorSetBindings::LightTransformsUbo),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &lightBufferInfo,
        .pTexelBufferView = nullptr
      }
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
      descriptorWrites.data(), 0, nullptr);
  }
}

void RenderResourcesImpl::createMainPassDescriptorSet()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_renderPassDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = m_descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
    .pSetLayouts = layouts.data()
  };

  m_mainPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_mainPassDescriptorSets.data()),
    "Failed to allocate descriptor sets");

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorBufferInfo cameraBufferInfo{
      .buffer = m_mainCameraUbo[i]->vkBuffer(),
      .offset = 0,
      .range = sizeof(CameraTransformsUbo)
    };

    VkDescriptorBufferInfo lightingBufferInfo{
      .buffer = m_lightingUbo[i]->vkBuffer(),
      .offset = 0,
      .range = sizeof(LightingUbo)
    };

    VkDescriptorImageInfo imageInfo{
      .sampler = m_shadowMapSampler,
      .imageView = m_shadowMapImage->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites{
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_mainPassDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::CameraTransformsUbo),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &cameraBufferInfo,
        .pTexelBufferView = nullptr
      },
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_mainPassDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::LightingUbo),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &lightingBufferInfo,
        .pTexelBufferView = nullptr
      },
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_mainPassDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::ShadowMap),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
      }
    };
  
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
      descriptorWrites.data(), 0, nullptr);

    m_logger.info(STR("Main pass descriptor set: " << m_mainPassDescriptorSets[i]));
  }
}

void RenderResourcesImpl::createOverlayPassDescriptorSet()
{
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_renderPassDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = m_descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
    .pSetLayouts = layouts.data()
  };

  m_overlayPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, m_overlayPassDescriptorSets.data()),
    "Failed to allocate descriptor sets");

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorBufferInfo cameraBufferInfo{
      .buffer = m_overlayCameraUbo[i]->vkBuffer(),
      .offset = 0,
      .range = sizeof(CameraTransformsUbo)
    };

    VkDescriptorImageInfo imageInfo{
      .sampler = m_shadowMapSampler,
      .imageView = m_shadowMapImage->vkImageView(), // TODO: Need this?
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites{
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_overlayPassDescriptorSets[i],
        .dstBinding = static_cast<uint32_t>(RenderPassDescriptorSetBindings::CameraTransformsUbo),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &cameraBufferInfo,
        .pTexelBufferView = nullptr
      }
    };
  
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
      descriptorWrites.data(), 0, nullptr);

    m_logger.info(STR("Render pass descriptor set: " << m_overlayPassDescriptorSets[i]));
  }
}

void RenderResourcesImpl::createTextureSampler()
{
  DBG_TRACE(m_logger);

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

  VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0.f,
    .anisotropyEnable = VK_FALSE, //VK_TRUE,
    .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.f,
    .maxLod = 0.f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler),
    "Failed to create texture sampler");
}

void RenderResourcesImpl::createNormalMapSampler()
{
  DBG_TRACE(m_logger);

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

  VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0.f,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.f,
    .maxLod = 0.f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_normalMapSampler),
    "Failed to create normal map sampler");
}

void RenderResourcesImpl::createCubeMapSampler()
{
  DBG_TRACE(m_logger);

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

  VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0.f,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.f,
    .maxLod = 0.f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_cubeMapSampler),
    "Failed to create normal map sampler");
}

RenderResourcesImpl::~RenderResourcesImpl()
{
  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_globalDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_renderPassDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_materialDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_objectDescriptorSetLayout, nullptr);

  vkDestroySampler(m_device, m_shadowMapSampler, nullptr);

  while (!m_meshes.empty()) {
    removeMesh(m_meshes.begin()->first);
  }
  vkDestroySampler(m_device, m_textureSampler, nullptr);
  vkDestroySampler(m_device, m_normalMapSampler, nullptr);
  vkDestroySampler(m_device, m_cubeMapSampler, nullptr);
  while (!m_materials.empty()) {
    removeMaterial(m_materials.begin()->first);
  }
  while (!m_textures.empty()) {
    removeTexture(m_textures.begin()->first);
  }
  while (!m_cubeMaps.empty()) {
    removeCubeMap(m_cubeMaps.begin()->first);
  }
}

} // namespace

RenderResourcesPtr createRenderResources(GpuBufferManager& bufferManager,
  VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue,
  VkCommandPool commandPool, Logger& logger)
{
  return std::make_unique<RenderResourcesImpl>(bufferManager, physicalDevice, device, graphicsQueue,
    commandPool, logger);
}

} // namespace render
} // namespace lithic3d
