#include "vulkan/render_resources.hpp"
#include "vulkan/vulkan_utils.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/trace.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/time.hpp"
#include "lithic3d/scoped_lock.hpp"
#include <map>
#include <array>
#include <cstring>
#include <cassert>

namespace lithic3d
{
namespace render
{
namespace
{

const Tick UNLOAD_DELAY = TICKS_PER_SECOND / 2;
const uint32_t SAMPLER_ARRAY_LEN = 5;

template<typename T, size_t N>
std::array<T, N> fillArray(const std::function<T()>& fn)
{
  std::array<T, N> arr;
  for (size_t i = 0; i < N; ++i) {
    arr[i] = fn();
  }
  return arr;
}

struct MeshData
{
  MeshPtr mesh = nullptr;
  GpuBufferPtr vertexBuffer;
  GpuBufferPtr indexBuffer;
  std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> instanceBuffer;
  uint32_t numInstances = 0;
  std::vector<VkDescriptorSet> objectDescriptorSets;
  std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> jointTransformsUbo;
};

using MeshDataPtr = std::unique_ptr<MeshData>;

struct TextureData
{
  //TexturePtr texture;
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
  CubeMapSampler = 3,
  SplatMapSampler = 4
};

enum class ObjectDescriptorSetBindings : uint32_t
{
  JointTransformsUbo = 0
};

class RenderResourcesImpl : public RenderResources
{
  public:
    RenderResourcesImpl(std::thread::id id, GpuBufferManager& bufferManager,
      VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, Logger& logger);

    void initialise() override;
    void update(uint32_t frame) override;

    // Descriptor sets
    //
    VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetNumber number) const override;
    VkDescriptorSet getGlobalDescriptorSet(size_t currentFrame) const override;
    VkDescriptorSet getRenderPassDescriptorSet(RenderPass renderPass,
      size_t currentFrame) const override;
    VkDescriptorSet getMaterialDescriptorSet(ResourceId id) const override;
    VkDescriptorSet getObjectDescriptorSet(ResourceId id, size_t currentFrame) const override;

    // Resources
    //
    void addTexture(ResourceId id, TexturePtr texture, bool genMipmaps) override;
    void addSplatMap(ResourceId id, TexturePtr texture) override;
    void addNormalMap(ResourceId id, TexturePtr texture, bool genMipmaps) override;
    void addCubeMap(ResourceId id, std::array<TexturePtr, 6> textures) override;
    void removeTexture(ResourceId id) override;
    void removeCubeMap(ResourceId id) override;

    // Meshes
    //
    void addMesh(ResourceId id, MeshPtr mesh) override;
    void removeMesh(ResourceId id) override;
    void updateJointTransforms(ResourceId meshId, const std::vector<Mat4x4f>& joints,
      size_t currentFrame) override;
    MeshBuffers getMeshBuffers(ResourceId id, size_t currentFrame) const override;
    void updateMeshInstances(ResourceId id, const std::vector<MeshInstance>& instances,
      size_t currentFrame) override;

    // Particles
    //
    const ParticleBuffers& getParticleBuffers() const override;

    // Materials
    //
    void addMaterial(ResourceId id, MaterialPtr material) override;
    void removeMaterial(ResourceId id) override;

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
    mutable std::recursive_mutex m_mutex; // Currently just protect everything with a single mutex
                                          // TODO: Better solution

    std::thread::id m_resourceThreadId;
    std::map<ResourceId, MeshDataPtr> m_meshes;
    std::map<ResourceId, TextureDataPtr> m_textures;
    std::map<ResourceId, CubeMapDataPtr> m_cubeMaps;
    std::map<ResourceId, MaterialDataPtr> m_materials;

    Logger& m_logger;
    GpuBufferManager& m_bufferManager;
    Scheduler m_scheduler;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    //VkCommandPool m_commandPool;
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
    VkSampler m_splatMapSampler;
    VkSampler m_cubeMapSampler;

    GpuImagePtr m_dummyTexture;

    VkExtent2D m_shadowMapSize{ SHADOW_MAP_W, SHADOW_MAP_H };
    GpuImagePtr m_shadowMapImage;
    VkSampler m_shadowMapSampler;

    VkDescriptorSetLayout m_particleCmpDescriptorSetLayout = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_particleCmpDescriptorSets;
    //std::array<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT> m_particleUbos;
    GpuBufferPtr m_particlesVertexBuffer;
    GpuBufferPtr m_particlesIndexBuffer;
    std::array<GpuBufferPtr, 2> m_particleSsbos;
    ParticleBuffers m_particleBuffers;

    inline void assertResourceThread() const;

    void createTextureSampler();
    void createNormalMapSampler();
    void createSplatMapSampler();
    void createCubeMapSampler();
    void createDescriptorPool();
    void addSamplerToDescriptorSet(VkDescriptorSet descriptorSet, uint32_t maxArrayLen,
      const std::vector<VkImageView>& imageViews, VkSampler sampler, uint32_t binding);

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

    void createDummyTexture();

    void createParticleBuffers();
    void createParticleDescriptorSetLayout();
    void createParticleDescriptorSets();
};

RenderResourcesImpl::RenderResourcesImpl(std::thread::id resourceThreadId,
  GpuBufferManager& bufferManager, VkPhysicalDevice physicalDevice, VkDevice device,
  VkCommandPool, // TODO: Never used?
  Logger& logger)
  : m_resourceThreadId(resourceThreadId)
  , m_logger(logger)
  , m_bufferManager(bufferManager)
  , m_physicalDevice(physicalDevice)
  , m_device(device)
//, m_commandPool(commandPool)
{
  DBG_TRACE(m_logger);

  // Don't do anthing here that winds up calling into GpuBufferManager::endSingleTimeCommands().
  // Put it in initialise() instead.
}

void RenderResourcesImpl::initialise()
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

  createDummyTexture();

  createParticleBuffers();
  createParticleDescriptorSetLayout();
  createParticleDescriptorSets();

  m_particleBuffers = {
    // TODO: Don't hard-code length 2 (should be MAX_FRAMES_IN_FLIGHT)
    //.ubos{ m_particleUbos[0]->vkBuffer(), m_particleUbos[1]->vkBuffer() },
    .vertexBuffer = m_particlesVertexBuffer->vkBuffer(),
    .indexBuffer = m_particlesIndexBuffer->vkBuffer(),
    .ssbos{ m_particleSsbos[0]->vkBuffer(), m_particleSsbos[1]->vkBuffer() },
    .cmpDescriptorSetLayout = m_particleCmpDescriptorSetLayout,
    .cmpDescriptorSets = m_particleCmpDescriptorSets,
    .numIndices = 6
  };
}

void RenderResourcesImpl::createDummyTexture()
{
  Texture texture{
    .width = 1,
    .height = 1,
    .channels = 4,
    .data = { 255, 0, 0, 255 }
  };
  m_dummyTexture = m_bufferManager.createTexture(texture, false);
}

inline void RenderResourcesImpl::assertResourceThread() const
{
  ASSERT(std::this_thread::get_id() == m_resourceThreadId, "Must be on resource manager thread");
}

void RenderResourcesImpl::update(uint32_t frame)
{
  DBG_TRACE(m_logger);

  m_scheduler.update();
  m_bufferManager.update(frame);
}

void RenderResourcesImpl::addTexture(ResourceId id, TexturePtr texture, bool genMipmaps)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  auto textureData = std::make_unique<TextureData>();
  textureData->image = m_bufferManager.createTexture(*texture, genMipmaps);
  //textureData->texture = std::move(texture);

  {
    SCOPED_LOCK(m_mutex);
    m_textures[id] = std::move(textureData);
  }
}

void RenderResourcesImpl::addSplatMap(ResourceId id, TexturePtr texture)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  auto textureData = std::make_unique<TextureData>();
  textureData->image = m_bufferManager.createTexture(*texture, false);
  //textureData->texture = std::move(texture);

  {
    SCOPED_LOCK(m_mutex);
    m_textures[id] = std::move(textureData);
  }
}

void RenderResourcesImpl::addNormalMap(ResourceId id, TexturePtr texture, bool genMipmaps)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  auto textureData = std::make_unique<TextureData>();
  textureData->image = m_bufferManager.createNormalMap(*texture, genMipmaps);
  //textureData->texture = std::move(texture);

  {
    SCOPED_LOCK(m_mutex);
    m_textures[id] = std::move(textureData);
  }
}

void RenderResourcesImpl::addCubeMap(ResourceId id, std::array<TexturePtr, 6> textures)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  auto cubeMapData = std::make_unique<CubeMapData>();
  cubeMapData->image = m_bufferManager.createCubeMap(textures);
  cubeMapData->textures = std::move(textures);

  {
    SCOPED_LOCK(m_mutex);
    m_cubeMaps[id] = std::move(cubeMapData);
  }
}

void RenderResourcesImpl::removeTexture(ResourceId id)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  m_scheduler.run(UNLOAD_DELAY, [this, id]() {
    SCOPED_LOCK(m_mutex);

    auto i = m_textures.find(id);
    if (i == m_textures.end()) {
      return;
    }

    m_textures.erase(i);
  });
}

void RenderResourcesImpl::removeCubeMap(ResourceId id)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  m_scheduler.run(UNLOAD_DELAY, [this, id]() {
    SCOPED_LOCK(m_mutex);

    auto i = m_cubeMaps.find(id);
    if (i == m_cubeMaps.end()) {
      return;
    }

    m_cubeMaps.erase(i);
  });
}

void RenderResourcesImpl::addMesh(ResourceId id, MeshPtr mesh)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  auto vertexArray = createVertexArray(*mesh);

  auto data = std::make_unique<MeshData>();
  data->mesh = std::move(mesh);
  data->vertexBuffer = m_bufferManager.createVertexBuffer(vertexArray.data(), vertexArray.size());
  data->indexBuffer = m_bufferManager.createIndexBuffer(data->mesh->indexBuffer.data.rawBytes(),
    data->mesh->indexBuffer.data.sizeInBytes());
  if (data->mesh->featureSet.flags.test(MeshFeatures::IsInstanced)) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      data->instanceBuffer[i] =
        m_bufferManager.createInstanceBuffer(data->mesh->maxInstances * sizeof(MeshInstance));
    }
  }

  if (data->mesh->featureSet.flags.test(MeshFeatures::IsAnimated)) {
    data->jointTransformsUbo = fillArray<GpuBufferPtr, MAX_FRAMES_IN_FLIGHT>([this]() {
      return m_bufferManager.createUbo(sizeof(JointTransformsUbo));
    });

    std::vector<Mat4x4f> joints(MAX_JOINTS, identityMatrix<4>());
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_bufferManager.writeToBuffer(*data->jointTransformsUbo[i],
        reinterpret_cast<const char*>(joints.data()), joints.size() * sizeof(Mat4x4f));
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

  {
    SCOPED_LOCK(m_mutex);
    m_meshes[id] = std::move(data);
  }
}

void RenderResourcesImpl::removeMesh(ResourceId id)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  m_scheduler.run(UNLOAD_DELAY, [this, id]() {
    SCOPED_LOCK(m_mutex);

    auto i = m_meshes.find(id);
    if (i == m_meshes.end()) {
      return;
    }

    if (i->second->objectDescriptorSets.size() > 0) {
      VK_CHECK(vkFreeDescriptorSets(m_device, m_descriptorPool,
        i->second->objectDescriptorSets.size(), i->second->objectDescriptorSets.data()),
        "Failed to free descriptor sets");
    }

    m_meshes.erase(i);
  });
}

MeshBuffers RenderResourcesImpl::getMeshBuffers(ResourceId id, size_t currentFrame) const
{
  SCOPED_LOCK(m_mutex);

  MAP_GET(iMesh, m_meshes, id);
  auto& mesh = iMesh->second;

  return {
    .vertexBuffer = mesh->vertexBuffer != nullptr ?
      mesh->vertexBuffer->vkBuffer() : VK_NULL_HANDLE,
    .indexBuffer = mesh->indexBuffer!= nullptr ?
      mesh->indexBuffer->vkBuffer() : VK_NULL_HANDLE,
    .instanceBuffer = mesh->instanceBuffer[currentFrame] != nullptr ?
      mesh->instanceBuffer[currentFrame]->vkBuffer() : VK_NULL_HANDLE,
    .numIndices = static_cast<uint32_t>(mesh->mesh->indexBuffer.data.numElements()),
    .numInstances = mesh->numInstances,
    .transform = mesh->mesh->transform
  };
}

const ParticleBuffers& RenderResourcesImpl::getParticleBuffers() const
{
  return m_particleBuffers;
}

// TODO: This is far too slow
void RenderResourcesImpl::updateMeshInstances(ResourceId id,
  const std::vector<MeshInstance>& instances, size_t currentFrame)
{
  DBG_TRACE(m_logger);

  SCOPED_LOCK(m_mutex);

  MAP_GET(iMesh, m_meshes, id);
  auto& mesh = *iMesh->second;
  ASSERT(mesh.mesh->featureSet.flags.test(MeshFeatures::IsInstanced),
    "Can't instance a non-instanced mesh");
  ASSERT(instances.size() <= mesh.mesh->maxInstances, "Max instances exceeded for this mesh");

  mesh.numInstances = static_cast<uint32_t>(instances.size());
  m_bufferManager.writeToBuffer(*mesh.instanceBuffer[currentFrame],
    reinterpret_cast<const char*>(instances.data()), instances.size() * sizeof(MeshInstance));
}

void RenderResourcesImpl::updateJointTransforms(ResourceId id, const std::vector<Mat4x4f>& joints,
  size_t currentFrame)
{
  DBG_TRACE(m_logger);
  DBG_ASSERT(joints.size() <= MAX_JOINTS, "Max number of joints exceeded");

  MAP_GET(iMesh, m_meshes, id);
  auto& mesh = *iMesh->second;
  m_bufferManager.writeToBuffer(*mesh.jointTransformsUbo[currentFrame],
    reinterpret_cast<const char*>(joints.data()), joints.size() * sizeof(Mat4x4f));
}

void RenderResourcesImpl::addSamplerToDescriptorSet(VkDescriptorSet descriptorSet,
  uint32_t maxArrayLen, const std::vector<VkImageView>& imageViews, VkSampler sampler,
  uint32_t binding)
{
  DBG_TRACE(m_logger);

  assert(imageViews.size() <= maxArrayLen);

  std::vector<VkDescriptorImageInfo> imageInfos;
  for (auto imageView : imageViews) {
    imageInfos.push_back(VkDescriptorImageInfo{
      .sampler = sampler,
      .imageView = imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
  }

  if (imageViews.size() > 0) {
    VkWriteDescriptorSet samplerDescriptorWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = descriptorSet,
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = imageInfos.data(),
      .pBufferInfo = nullptr,
      .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(m_device, 1, &samplerDescriptorWrite, 0, nullptr);
  }

  uint32_t empty = maxArrayLen - imageViews.size();
  if (empty > 0) {
    std::vector<VkDescriptorImageInfo> dummyImageInfos;
    for (uint32_t i = 0; i < empty; ++i) {
      dummyImageInfos.push_back(VkDescriptorImageInfo{
        .sampler = m_textureSampler,
        .imageView = m_dummyTexture->vkImageView(0),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      });
    }

    VkWriteDescriptorSet samplerDescriptorWrite{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = descriptorSet,
      .dstBinding = binding,
      .dstArrayElement = static_cast<uint32_t>(imageViews.size()),
      .descriptorCount = static_cast<uint32_t>(dummyImageInfos.size()),
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = dummyImageInfos.data(),
      .pBufferInfo = nullptr,
      .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(m_device, 1, &samplerDescriptorWrite, 0, nullptr);
  }
}

void RenderResourcesImpl::addMaterial(ResourceId id, MaterialPtr material)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

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

  m_bufferManager.writeToBuffer(*materialData->ubo, reinterpret_cast<const char*>(&ubo),
    sizeof(ubo));

  //if (material->featureSet.flags.test(MaterialFeatures::HasTexture)) {
    std::vector<VkImageView> imageViews;
    for (auto& texture : material->textures) {
      MAP_GET(i, m_textures, texture.id());
      imageViews.push_back(i->second->image->vkImageView(0));
    }
    addSamplerToDescriptorSet(materialData->descriptorSet, SAMPLER_ARRAY_LEN, imageViews,
      m_textureSampler, static_cast<uint32_t>(MaterialDescriptorSetBindings::TextureSampler));
  //}
  if (material->featureSet.flags.test(MaterialFeatures::HasNormalMap)) {
    std::vector<VkImageView> imageViews;
    for (auto& normalMap : material->normalMaps) {
      MAP_GET(i, m_textures, normalMap.id());
      imageViews.push_back(i->second->image->vkImageView(0));
    }
    addSamplerToDescriptorSet(materialData->descriptorSet, SAMPLER_ARRAY_LEN, imageViews,
      m_normalMapSampler, static_cast<uint32_t>(MaterialDescriptorSetBindings::NormapMapSampler));
  }
  if (material->featureSet.flags.test(MaterialFeatures::HasCubeMap)) {
    MAP_GET(i, m_cubeMaps, material->cubeMap.id());
    VkImageView imageView = i->second->image->vkImageView(0);
    addSamplerToDescriptorSet(materialData->descriptorSet, 1, { imageView }, m_cubeMapSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::CubeMapSampler));
  }
  if (material->splatMap) {
    MAP_GET(i, m_textures, material->splatMap.id());
    auto imageView = i->second->image->vkImageView(0);
    addSamplerToDescriptorSet(materialData->descriptorSet, 1, { imageView }, m_splatMapSampler,
      static_cast<uint32_t>(MaterialDescriptorSetBindings::SplatMapSampler));
  }

  {
    SCOPED_LOCK(m_mutex);

    materialData->material = std::move(material);
    m_materials[id] = std::move(materialData);
  }
}

void RenderResourcesImpl::removeMaterial(ResourceId id)
{
  DBG_TRACE(m_logger);
  assertResourceThread();

  m_scheduler.run(UNLOAD_DELAY, [this, id]() {
    SCOPED_LOCK(m_mutex);

    auto i = m_materials.find(id);
    if (i == m_materials.end()) {
      return;
    }

    VK_CHECK(vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &i->second->descriptorSet),
      "Failed to free descriptor set");

    m_materials.erase(i);
  });
}

VkDescriptorSet RenderResourcesImpl::getMaterialDescriptorSet(ResourceId id) const
{
  SCOPED_LOCK(m_mutex);

  if (id == NULL_RESOURCE_ID) {
    return VK_NULL_HANDLE;
  }

  MAP_GET(i, m_materials, id);
  return i->second->descriptorSet;
}

VkDescriptorSet RenderResourcesImpl::getObjectDescriptorSet(ResourceId id,
  size_t currentFrame) const
{
  SCOPED_LOCK(m_mutex);

  auto i = m_meshes.find(id);
  if (i == m_meshes.end()) {
    return VK_NULL_HANDLE;
  }

  auto& mesh = *i->second;
  return mesh.objectDescriptorSets.size() > 0 ?
    mesh.objectDescriptorSets[currentFrame] :
    VK_NULL_HANDLE;
}

void RenderResourcesImpl::updateMainCameraUbo(const CameraTransformsUbo& ubo,
  size_t currentFrame)
{
  DBG_TRACE(m_logger);

  m_bufferManager.writeToBuffer(*m_mainCameraUbo[currentFrame], reinterpret_cast<const char*>(&ubo),
    sizeof(ubo));
}

void RenderResourcesImpl::updateOverlayCameraUbo(const CameraTransformsUbo& ubo,
  size_t currentFrame)
{
  DBG_TRACE(m_logger);

  m_bufferManager.writeToBuffer(*m_overlayCameraUbo[currentFrame],
    reinterpret_cast<const char*>(&ubo), sizeof(ubo));
}

void RenderResourcesImpl::updateLightTransformsUbo(const LightTransformsUbo& ubo,
  size_t currentFrame)
{
  DBG_TRACE(m_logger);

  m_bufferManager.writeToBuffer(*m_lightTransformsUbo[currentFrame],
    reinterpret_cast<const char*>(&ubo), sizeof(ubo));
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
  SCOPED_LOCK(m_mutex);

  return m_globalDescriptorSets[currentFrame];
}

void RenderResourcesImpl::updateLightingUbo(const LightingUbo& ubo, size_t currentFrame)
{
  DBG_TRACE(m_logger);

  m_bufferManager.writeToBuffer(*m_lightingUbo[currentFrame], reinterpret_cast<const char*>(&ubo),
    sizeof(ubo));
}

VkDescriptorSet RenderResourcesImpl::getRenderPassDescriptorSet(RenderPass renderPass,
  size_t currentFrame) const
{
  SCOPED_LOCK(m_mutex);

  switch (renderPass) {
    case RenderPass::Main: return m_mainPassDescriptorSets[currentFrame];
    case RenderPass::Shadow0: return m_mainPassDescriptorSets[currentFrame];  // TODO
    case RenderPass::Shadow1: return m_mainPassDescriptorSets[currentFrame];  // TODO
    case RenderPass::Shadow2: return m_mainPassDescriptorSets[currentFrame];  // TODO
    case RenderPass::Ssr: return m_mainPassDescriptorSets[currentFrame];      // TODO
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
}

void RenderResourcesImpl::createShadowMap()
{
  DBG_TRACE(m_logger);

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

  // TODO: Rationalise these values or make them configurable

  std::array<VkDescriptorPoolSize, 3> poolSizes{
    VkDescriptorPoolSize{
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 10000
    },
    VkDescriptorPoolSize{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 10000
    },
    VkDescriptorPoolSize{
      .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1000
    }
  };

  VkDescriptorPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = 10000,
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
  createSplatMapSampler();
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
    .descriptorCount = SAMPLER_ARRAY_LEN,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  VkDescriptorSetLayoutBinding normalMapLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::NormapMapSampler),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = SAMPLER_ARRAY_LEN,
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

  VkDescriptorSetLayoutBinding splatMapSamplerLayoutBinding{
    .binding = static_cast<uint32_t>(MaterialDescriptorSetBindings::SplatMapSampler),
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
  };

  std::vector<VkDescriptorSetLayoutBinding> bindings{
    textureSamplerLayoutBinding,
    normalMapLayoutBinding,
    cubeMapLayoutBinding,
    uboLayoutBinding,
    splatMapSamplerLayoutBinding
    // ...
  };

  // TODO: Remove this?
/*
  std::array<VkDescriptorBindingFlags, 5> bindingFlags = {
    0,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    0,
    0
  };

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .pNext = nullptr,
    .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
    .pBindingFlags = bindingFlags.data()
  };*/

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
  DBG_TRACE(m_logger);

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
  DBG_TRACE(m_logger);

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

    std::vector<VkWriteDescriptorSet> descriptorWrites;

    descriptorWrites.push_back({
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
    });
    descriptorWrites.push_back({
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
    });

    VkDescriptorImageInfo imageInfo{
      .sampler = m_shadowMapSampler,
      .imageView = m_shadowMapImage->vkImageView(0),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    descriptorWrites.push_back({
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
    });
  
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
      descriptorWrites.data(), 0, nullptr);

    m_logger.info(STR("Main pass descriptor set: " << m_mainPassDescriptorSets[i]));
  }
}

void RenderResourcesImpl::createOverlayPassDescriptorSet()
{
  DBG_TRACE(m_logger);

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

// TODO: Factor out common code from these create...Sampler() functions

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
    .maxLod = 32, // TODO: This OK?
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
  };

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler),
    "Failed to create texture sampler");
}

void RenderResourcesImpl::createSplatMapSampler()
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
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
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

  VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_splatMapSampler),
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

void RenderResourcesImpl::createParticleBuffers()
{
  //for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
  //  m_particleUbos[i] = m_bufferManager.createUbo(sizeof(ParticlesUbo));
  //}

  Mesh mesh;
  mesh.featureSet = MeshFeatureSet{
    .vertexLayout = {
      BufferUsage::AttrPosition,
      BufferUsage::AttrTexCoord
    },
    .flags{}
  };
  mesh.attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec3f>{
      { 0, 0, 0 },
      { 1, 0, 0 },
      { 1, 1, 0 },
      { 0, 1, 0 }
    }}, BufferUsage::AttrPosition
  });
  mesh.attributeBuffers.emplace_back(Buffer{AlignedBytes{
    std::vector<Vec2f>{
      { 0, 0 },
      { 1, 0 },
      { 1, 1 },
      { 0, 1 }
    }}, BufferUsage::AttrTexCoord
  });
  mesh.indexBuffer = Buffer{AlignedBytes{
    std::vector<uint16_t>{
      0, 1, 2, 0, 2, 3
    }},
    BufferUsage::Index
  };

  auto vertexArray = createVertexArray(mesh);
  m_particlesIndexBuffer = m_bufferManager.createIndexBuffer(mesh.indexBuffer.data.rawBytes(),
    mesh.indexBuffer.data.sizeInBytes());

  m_particlesVertexBuffer = m_bufferManager.createVertexBuffer(vertexArray.data(),
    vertexArray.size());

  std::vector<Particle> particles(PARTICLE_COUNT);

  for (auto& particle : particles) {
    particle.position = { 0.f, 0.f, 0.f };
    particle.velocity = { 0.f, 0.f, 0.f };
    particle.colour = { 1.f, 0.5f, 0.5f, 1.f }; // TODO
  }

  m_particleSsbos = {
    m_bufferManager.createParticleBuffer(reinterpret_cast<const char*>(particles.data()),
      particles.size() * sizeof(Particle)),
    // TODO: Shouldn't need to fill second buffer
    m_bufferManager.createParticleBuffer(reinterpret_cast<const char*>(particles.data()),
      particles.size() * sizeof(Particle))
  };
}

// Specifically, this is for the compute shader. TODO: Rename?
void RenderResourcesImpl::createParticleDescriptorSetLayout()
{
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{/*
    VkDescriptorSetLayoutBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .pImmutableSamplers = nullptr
    },*/
    VkDescriptorSetLayoutBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .pImmutableSamplers = nullptr
    },
    VkDescriptorSetLayoutBinding{
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .pImmutableSamplers = nullptr
    }
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
    .pBindings = layoutBindings.data()
  };

  VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
    &m_particleCmpDescriptorSetLayout), "Failed to create descriptor set layout");
}

void RenderResourcesImpl::createParticleDescriptorSets()
{
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = m_descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &m_particleCmpDescriptorSetLayout
    };

    VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &m_particleCmpDescriptorSets[i]),
      "Failed to allocate descriptor set");
/*
    VkDescriptorBufferInfo uniformBufferInfo{
      .buffer = m_particleUbos[i]->vkBuffer(),
      .offset = 0,
      .range = m_particleUbos[i]->size()
    };*/

    VkDescriptorBufferInfo ssboInfoLastFrame{
      .buffer = m_particleSsbos[wrap(i - 1, 0, MAX_FRAMES_IN_FLIGHT - 1)]->vkBuffer(),
      .offset = 0,
      .range = sizeof(Particle) * PARTICLE_COUNT
    };

    VkDescriptorBufferInfo ssboInfoCurrentFrame{
      .buffer = m_particleSsbos[i]->vkBuffer(),
      .offset = 0,
      .range = sizeof(Particle) * PARTICLE_COUNT
    };

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{
/*
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_particleCmpDescriptorSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &uniformBufferInfo,
        .pTexelBufferView = nullptr
      },*/
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_particleCmpDescriptorSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &ssboInfoLastFrame,
        .pTexelBufferView = nullptr
      },
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_particleCmpDescriptorSets[i],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &ssboInfoCurrentFrame,
        .pTexelBufferView = nullptr
      }
    };

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()),
      descriptorWrites.data(), 0, nullptr);
  }
}

RenderResourcesImpl::~RenderResourcesImpl()
{
  DBG_TRACE(m_logger);

  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_globalDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_renderPassDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_materialDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_objectDescriptorSetLayout, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_particleCmpDescriptorSetLayout, nullptr);

  vkDestroySampler(m_device, m_shadowMapSampler, nullptr);

  vkDestroySampler(m_device, m_textureSampler, nullptr);
  vkDestroySampler(m_device, m_splatMapSampler, nullptr);
  vkDestroySampler(m_device, m_normalMapSampler, nullptr);
  vkDestroySampler(m_device, m_cubeMapSampler, nullptr);

  if (!m_meshes.empty()) {
    m_logger.warn("Mesh not destroyed");
  }

  if (!m_materials.empty()) {
    m_logger.warn("Material not destroyed");
  }

  if (!m_textures.empty()) {
    m_logger.warn("Texture not destroyed");
  }

  if (!m_cubeMaps.empty()) {
    m_logger.warn("Cube map not destroyed");
  }
}

} // namespace

RenderResourcesPtr createRenderResources(std::thread::id resourceThreadId,
  GpuBufferManager& bufferManager, VkPhysicalDevice physicalDevice, VkDevice device,
  VkCommandPool commandPool, Logger& logger)
{
  return std::make_unique<RenderResourcesImpl>(resourceThreadId, bufferManager, physicalDevice,
    device, commandPool, logger);
}

} // namespace render
} // namespace lithic3d
