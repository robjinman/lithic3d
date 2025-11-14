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
  VkImage image;
  VkDeviceMemory imageMemory;
  VkImageView imageView;
};

using TextureDataPtr = std::unique_ptr<TextureData>;

struct CubeMapData
{
  std::array<TexturePtr, 6> textures;
  VkImage image;
  VkDeviceMemory imageMemory;
  VkImageView imageView;
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
    VkImage getShadowMapImage() const override;
    VkImageView getShadowMapImageView() const override;

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
    VkImage m_shadowMapImage;
    VkDeviceMemory m_shadowMapImageMemory;
    VkImageView m_shadowMapImageView;
    VkSampler m_shadowMapSampler;

    RenderItemId addTexture(TexturePtr texture, VkFormat format);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
      VkDeviceSize bufferOffset, uint32_t layer);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
      VkBuffer& buffer, VkDeviceMemory& bufferMemory); // TODO: Delete
    void updateInstanceBuffer(const std::vector<MeshInstance>& instanceData, VkBuffer buffer);
    void createTextureSampler();
    void createNormalMapSampler();
    void createCubeMapSampler();
    void createDescriptorPool();
    void addSamplerToDescriptorSet(VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler,
      uint32_t binding);

    void createGlobalDescriptorSetLayout();
    void createRenderPassDescriptorSetLayout();
    void createMaterialDescriptorSetLayout();
    void createObjectDescriptorSetLayout();

    void createGlobalDescriptorSet();
    void createMainPassDescriptorSet();
    void createOverlayPassDescriptorSet();
    //void createShadowPassDescriptorSet();

    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
      uint32_t layerCount);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
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

  createDescriptorPool();
  createMaterialDescriptorSetLayout();
  createGlobalDescriptorSet();
  createRenderPassDescriptorSetLayout();
  createMainPassDescriptorSet();
  createOverlayPassDescriptorSet();
  //createShadowPassDescriptorSet();
  createObjectDescriptorSetLayout();
}

RenderItemId RenderResourcesImpl::addTexture(TexturePtr texture, VkFormat format)
{
  static RenderItemId nextTextureId = 1;

  auto textureData = std::make_unique<TextureData>();

  ASSERT(texture->data.size() % 4 == 0, "Texture data size should be multiple of 4");

  VkDeviceSize imageSize = texture->data.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
    stagingBufferMemory);

  void* data = nullptr;
  vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, texture->data.data(), static_cast<size_t>(imageSize));
  vkUnmapMemory(m_device, stagingBufferMemory);

  createImage(m_physicalDevice, m_device, texture->width, texture->height, format,
    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureData->image, textureData->imageMemory);

  transitionImageLayout(textureData->image, VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

  copyBufferToImage(stagingBuffer, textureData->image, texture->width, texture->height, 0, 0);

  transitionImageLayout(textureData->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

  vkDestroyBuffer(m_device, stagingBuffer, nullptr);
  vkFreeMemory(m_device, stagingBufferMemory, nullptr);

  textureData->imageView = createImageView(m_device, textureData->image, format,
    VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);

  auto textureId = nextTextureId++;
  textureData->texture = std::move(texture);
  m_textures[textureId] = std::move(textureData);

  return textureId;
}

RenderItemId RenderResourcesImpl::addTexture(TexturePtr texture)
{
  return addTexture(std::move(texture), VK_FORMAT_R8G8B8A8_SRGB);
}

RenderItemId RenderResourcesImpl::addNormalMap(TexturePtr texture)
{
  return addTexture(std::move(texture), VK_FORMAT_R8G8B8A8_UNORM);
}

RenderItemId RenderResourcesImpl::addCubeMap(std::array<TexturePtr, 6> textures)
{
  auto cubeMapData = std::make_unique<CubeMapData>();

  VkDeviceSize imageSize = textures[0]->data.size();
  VkDeviceSize cubeMapSize = imageSize * 6;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  createBuffer(cubeMapSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
    stagingBufferMemory);

  //auto stagingBuffer = m_bufferManager.createStagingBuffer(cubeMapSize);

  uint32_t width = textures[0]->width;
  uint32_t height = textures[0]->height;

  uint8_t* data = nullptr;
  vkMapMemory(m_device, stagingBufferMemory, 0, cubeMapSize, 0, reinterpret_cast<void**>(&data));
  for (size_t i = 0; i < 6; ++i) {
    ASSERT(textures[i]->data.size() % 4 == 0, "Texture data size should be multiple of 4");
    ASSERT(textures[i]->width == width, "Cube map images should have same size");
    ASSERT(textures[i]->height == height, "Cube map images should have same size");

    size_t offset = i * imageSize;
    memcpy(data + offset, textures[i]->data.data(), static_cast<size_t>(imageSize));

    //memcpy(stagingBuffer->mappedAddress() + offset, textures[i]->data.data(),
    //  static_cast<size_t>(imageSize));
  }
  vkUnmapMemory(m_device, stagingBufferMemory);

  createImage(m_physicalDevice, m_device, width, height, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cubeMapData->image, cubeMapData->imageMemory, 6,
    VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

  transitionImageLayout(cubeMapData->image, VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);

  // TODO: Use GpuBufferManager
  for (uint32_t i = 0; i < 6; ++i) {
    VkDeviceSize offset = i * imageSize;
    copyBufferToImage(stagingBuffer, cubeMapData->image, width, height, offset, i);
  }

  transitionImageLayout(cubeMapData->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

  vkDestroyBuffer(m_device, stagingBuffer, nullptr);
  vkFreeMemory(m_device, stagingBufferMemory, nullptr);

  cubeMapData->imageView = createImageView(m_device, cubeMapData->image, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6);

  static RenderItemId nextCubeMapId = 1;

  auto cubeMapId = nextCubeMapId++;
  cubeMapData->textures = std::move(textures);
  m_cubeMaps[cubeMapId] = std::move(cubeMapData);

  return cubeMapId;
}

void RenderResourcesImpl::removeTexture(RenderItemId id)
{
  auto i = m_textures.find(id);
  if (i == m_textures.end()) {
    return;
  }

  vkDestroyImageView(m_device, i->second->imageView, nullptr);
  vkDestroyImage(m_device, i->second->image, nullptr);
  vkFreeMemory(m_device, i->second->imageMemory, nullptr);

  m_textures.erase(i);
}

void RenderResourcesImpl::removeCubeMap(RenderItemId id)
{
  auto i = m_cubeMaps.find(id);
  if (i == m_cubeMaps.end()) {
    return;
  }

  vkDestroyImageView(m_device, i->second->imageView, nullptr);
  vkDestroyImage(m_device, i->second->image, nullptr);
  vkFreeMemory(m_device, i->second->imageMemory, nullptr);

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

    std::vector<Mat4x4f> joints(MAX_JOINTS, identityMatrix<float, 4>());
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
  updateInstanceBuffer(instances, mesh->instanceBuffer->vkBuffer());
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
    VkImageView imageView = m_textures.at(material->texture.id)->imageView;
    addSamplerToDescriptorSet(materialData->descriptorSet, imageView, m_textureSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::TextureSampler));
  }
  if (material->featureSet.flags.test(MaterialFeatures::HasNormalMap)) {
    VkImageView imageView = m_textures.at(material->normalMap.id)->imageView;
    addSamplerToDescriptorSet(materialData->descriptorSet, imageView, m_normalMapSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::NormapMapSampler));
  }
  if (material->featureSet.flags.test(MaterialFeatures::HasCubeMap)) {
    VkImageView imageView = m_cubeMaps.at(material->cubeMap.id)->imageView;
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

VkImageView RenderResourcesImpl::getShadowMapImageView() const
{
  return m_shadowMapImageView;
}

VkImage RenderResourcesImpl::getShadowMapImage() const
{
  return m_shadowMapImage;
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

void RenderResourcesImpl::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
  uint32_t height, VkDeviceSize bufferOffset, uint32_t layer)
{
  DBG_TRACE(m_logger);

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region{
    .bufferOffset = bufferOffset,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = layer,
      .layerCount = 1
    },
    .imageOffset = { 0, 0, 0 },
    .imageExtent = { width, height, 1 }
  };

  vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
    &region);

  endSingleTimeCommands(commandBuffer);
}

void RenderResourcesImpl::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
  DBG_TRACE(m_logger);

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{
    .srcOffset = 0,
    .dstOffset = 0,
    .size = size
  };

  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}

// TODO: Remove
void RenderResourcesImpl::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
  VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
  DBG_TRACE(m_logger);

  VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr
  };

  VK_CHECK(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = nullptr,
    .allocationSize = memRequirements.size,
    .memoryTypeIndex = findMemoryType(m_physicalDevice, memRequirements.memoryTypeBits, properties)
  };

  VK_CHECK(vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory),
    "Failed to allocate memory for buffer");

  vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void RenderResourcesImpl::updateInstanceBuffer(const std::vector<MeshInstance>& instances,
  VkBuffer buffer)
{
  DBG_TRACE(m_logger);

  VkDeviceSize size = sizeof(MeshInstance) * instances.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
    stagingBufferMemory);

  // TODO: Consider partial updates?

  void* data = nullptr;
  vkMapMemory(m_device, stagingBufferMemory, 0, size, 0, &data);
  memcpy(data, instances.data(), size);
  vkUnmapMemory(m_device, stagingBufferMemory);

  copyBuffer(stagingBuffer, buffer, size);

  vkDestroyBuffer(m_device, stagingBuffer, nullptr);
  vkFreeMemory(m_device, stagingBufferMemory, nullptr);
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
  VkFormat depthFormat = findDepthFormat(m_physicalDevice);

  createImage(m_physicalDevice, m_device, m_shadowMapSize.width, m_shadowMapSize.height,
    depthFormat, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_shadowMapImage, m_shadowMapImageMemory);

  m_shadowMapImageView = createImageView(m_device, m_shadowMapImage, depthFormat,
    VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);

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
      .imageView = m_shadowMapImageView,
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
  VkFormat depthFormat = findDepthFormat(m_physicalDevice);

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
      .imageView = m_shadowMapImageView,
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

VkCommandBuffer RenderResourcesImpl::beginSingleTimeCommands()
{
  VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = m_commandPool, // TODO: Separate pool for temp buffers?
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
  };
  
  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr
  };

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void RenderResourcesImpl::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = nullptr,
    .waitSemaphoreCount = 0,
    .pWaitSemaphores = nullptr,
    .pWaitDstStageMask = nullptr,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
    .signalSemaphoreCount = 0,
    .pSignalSemaphores = nullptr
  };
  
  vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_graphicsQueue); // TODO: Submit commands asynchronously (see p201)

  vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void RenderResourcesImpl::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
  VkImageLayout newLayout, uint32_t layerCount)
{
  DBG_TRACE(m_logger);

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = nullptr,
    .srcAccessMask = 0,
    .dstAccessMask = 0,
    .oldLayout = oldLayout,
    .newLayout = newLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = layerCount
    }
  };

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
    newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else {
    EXCEPTION("Unsupported layout transition");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1,
    &barrier);

  endSingleTimeCommands(commandBuffer);
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
  vkDestroyImageView(m_device, m_shadowMapImageView, nullptr);
  vkDestroyImage(m_device, m_shadowMapImage, nullptr);
  vkFreeMemory(m_device, m_shadowMapImageMemory, nullptr);

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
