#pragma once

#include "lithic3d/renderer.hpp"
#include "vulkan/gpu_buffer_manager.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>

namespace lithic3d
{

class Logger;
class WorkQueue;

namespace render
{

const uint32_t MAX_POINT_LIGHTS = 4;
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
  Mat4x4f viewMatrix[NUM_SHADOW_MAPS];
  Mat4x4f projMatrix[NUM_SHADOW_MAPS];
};

struct Light
{
  Vec3f worldPos;
  uint8_t _pad0[4];
  Vec3f colour;
  float ambient;
  float specular;
  uint8_t _pad1[12];
};

struct LightingUbo
{
  Vec3f viewPos;
  uint32_t numPointLights;
  Light lights[MAX_POINT_LIGHTS];
  Light directionalLight;
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
  Mat4x4f transform;
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
    // Threads: render
    virtual void initialise() = 0;
    virtual void update(uint32_t frame) = 0;

    // Textures
    //
    // Threads: resource
    virtual void addTexture(ResourceId id, TexturePtr texture) = 0;
    // Threads: resource
    virtual void addNormalMap(ResourceId id, TexturePtr texture) = 0;
    // Threads: resource
    virtual void addCubeMap(ResourceId id, std::array<TexturePtr, 6> textures) = 0;
    // Threads: resource
    virtual void addSplatMap(ResourceId id, TexturePtr texture) = 0;
    // Threads: resource
    virtual void removeTexture(ResourceId id) = 0;
    // Threads: resource
    virtual void removeCubeMap(ResourceId id) = 0;

    // Descriptor sets
    //
    // Threads: render if running, main otherwise
    virtual VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetNumber number) const = 0;
    // Threads: render
    virtual VkDescriptorSet getGlobalDescriptorSet(size_t currentFrame) const = 0;
    // Threads: render
    virtual VkDescriptorSet getRenderPassDescriptorSet(RenderPass renderpass,
      size_t currentFrame) const = 0;
    // Threads: render
    virtual VkDescriptorSet getMaterialDescriptorSet(ResourceId id) const = 0;
    // Threads: render
    virtual VkDescriptorSet getObjectDescriptorSet(ResourceId id, size_t currentFrame) const = 0;

    // Meshes
    //
    // Threads: resource
    virtual void addMesh(ResourceId id, MeshPtr mesh) = 0;
    // Threads: resource
    virtual void removeMesh(ResourceId id) = 0;
    // Threads: render
    virtual void updateJointTransforms(ResourceId meshId, const std::vector<Mat4x4f>& joints,
      size_t currentFrame) = 0;
    // Threads: render
    virtual MeshBuffers getMeshBuffers(ResourceId id) const = 0;
    // Threads: render
    virtual void updateMeshInstances(ResourceId id, const std::vector<MeshInstance>& instances) = 0;

    // Materials
    //
    // Threads: resource
    virtual void addMaterial(ResourceId id, MaterialPtr material) = 0;
    // Threads: resource
    virtual void removeMaterial(ResourceId id) = 0;

    // Transforms
    //
    // Threads: render
    virtual void updateMainCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) = 0;
    // Threads: render
    virtual void updateOverlayCameraUbo(const CameraTransformsUbo& ubo, size_t currentFrame) = 0;
    // Threads: render
    virtual void updateLightTransformsUbo(const LightTransformsUbo& ubo, size_t currentFrame) = 0;

    // Lighting
    //
    // Threads: render
    virtual void updateLightingUbo(const LightingUbo& ubo, size_t currentFrame) = 0;

    // Shadow pass
    //
    // Threads: render
    virtual GpuImage& getShadowMap() = 0;

    // Threads: main
    virtual ~RenderResources() = default;
};

using RenderResourcesPtr = std::unique_ptr<RenderResources>;

class GpuBufferManager;

RenderResourcesPtr createRenderResources(std::thread::id resourceThreadId,
  GpuBufferManager& bufferManager, VkPhysicalDevice physicalDevice, VkDevice device,
  VkCommandPool commandPool, Logger& logger);

} // namespace render
} // namespace lithic3d
