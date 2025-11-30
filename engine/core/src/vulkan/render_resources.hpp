#pragma once

#include "lithic3d/renderer.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>

namespace lithic3d
{

class FileSystem;
class Logger;

namespace render
{

const uint32_t MAX_LIGHTS = 4;
const uint32_t SHADOW_MAP_W = 4096;
const uint32_t SHADOW_MAP_H = 4096;
const uint32_t MAX_JOINTS = 128;

// TODO: Hide these inside cpp file?
#pragma pack(push, 4)
struct CameraTransformsUbo
{
  Mat4x4f viewMatrix;
  Mat4x4f projMatrix;
};

struct LightTransformsUbo
{
  Mat4x4f viewMatrix;
  Mat4x4f projMatrix;
};

struct Light
{
  Vec3f worldPos;
  uint8_t _pad0[4];
  Vec3f colour;
  float ambient;
  float specular;
  uint8_t _pad2[12];
};

struct LightingUbo
{
  Vec3f viewPos;
  uint32_t numLights;
  Light lights[MAX_LIGHTS];
};

struct MaterialUbo
{
  Vec4f colour;
  // TODO: PBR properties
};

struct JointTransformsUbo
{
  Mat4x4f transforms[MAX_JOINTS];
};

struct MeshInstance
{
  Mat4x4f modelMatrix;
};
#pragma pack(pop)

struct MeshBuffers
{
  VkBuffer vertexBuffer;
  VkBuffer indexBuffer;
  VkBuffer instanceBuffer;
  uint32_t numIndices;
  uint32_t numInstances;
};

enum class DescriptorSetNumber : uint32_t
{
  Global = 0,
  RenderPass = 1,
  Material = 2,
  Object = 3
};

class GpuImage;

class RenderResources
{
  public:
    // Textures
    //
    virtual void addTexture(ResourceId id, TexturePtr texture) = 0;
    virtual void addNormalMap(ResourceId id, TexturePtr texture) = 0;
    virtual void addCubeMap(ResourceId id, std::array<TexturePtr, 6> textures) = 0;
    virtual void removeTexture(ResourceId id) = 0;
    virtual void removeCubeMap(ResourceId id) = 0;

    // Descriptor sets
    //
    virtual VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetNumber number) const = 0;
    virtual VkDescriptorSet getGlobalDescriptorSet(size_t currentFrame) const = 0;
    virtual VkDescriptorSet getRenderPassDescriptorSet(RenderPass renderpass,
      size_t currentFrame) const = 0;
    virtual VkDescriptorSet getMaterialDescriptorSet(ResourceId id) const = 0;
    virtual VkDescriptorSet getObjectDescriptorSet(ResourceId id, size_t currentFrame) const = 0;

    // Meshes
    //
    virtual void addMesh(ResourceId id, MeshPtr mesh) = 0;
    virtual void removeMesh(ResourceId id) = 0;
    virtual void updateJointTransforms(ResourceId meshId, const std::vector<Mat4x4f>& joints,
      size_t currentFrame) = 0;
    virtual MeshBuffers getMeshBuffers(ResourceId id) const = 0;
    virtual void updateMeshInstances(ResourceId id, const std::vector<MeshInstance>& instances) = 0;
    virtual const MeshFeatureSet& getMeshFeatures(ResourceId id) const = 0;

    // Materials
    //
    virtual void addMaterial(ResourceId id, MaterialPtr material) = 0;
    virtual void removeMaterial(ResourceId id) = 0;
    virtual const MaterialFeatureSet& getMaterialFeatures(ResourceId id) const = 0;

    // Transforms
    //
    // > Camera
    virtual void updateMainCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) = 0;
    virtual void updateOverlayCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) = 0;
    // > Light
    virtual void updateLightTransformsUbo(const LightTransformsUbo& ubo, size_t currentFrame) = 0;

    // Lighting
    //
    virtual void updateLightingUbo(const LightingUbo& ubo, size_t currentFrame) = 0;

    // Shadow pass
    //
    virtual GpuImage& getShadowMap() = 0;

    virtual ~RenderResources() = default;
};

using RenderResourcesPtr = std::unique_ptr<RenderResources>;

class GpuBufferManager;

RenderResourcesPtr createRenderResources(GpuBufferManager& bufferManager,
  VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue,
  VkCommandPool commandPool, Logger& logger);

} // namespace render
} // namespace lithic3d
