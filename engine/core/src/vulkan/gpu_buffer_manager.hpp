#pragma once

#include "lithic3d/renderables.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <ostream>

namespace lithic3d
{

const uint32_t SHADOW_MAP_W = 2048;
const uint32_t SHADOW_MAP_H = 2048;
const uint32_t NUM_SHADOW_MAPS = 3;

class Logger;
class WorkQueue;

namespace render
{

class GpuBuffer
{
  public:
    virtual size_t size() const = 0;
    virtual char* mappedAddress() = 0;
    virtual VkBuffer vkBuffer() = 0;

    virtual ~GpuBuffer() = default;
};

using GpuBufferPtr = std::unique_ptr<GpuBuffer>;

class GpuImage
{
  public:
    virtual VkImage vkImage() = 0;
    virtual VkImageView vkImageView(uint32_t index) = 0;

    virtual ~GpuImage() = default;
};

using GpuImagePtr = std::unique_ptr<GpuImage>;

class GpuBufferManager
{
  public:
    virtual GpuBufferPtr createUbo(size_t size) = 0;
    virtual GpuBufferPtr createVertexBuffer(const char* data, size_t size) = 0;
    virtual GpuBufferPtr createIndexBuffer(const char* data, size_t size) = 0;
    virtual GpuBufferPtr createInstanceBuffer(size_t size) = 0;
    virtual GpuBufferPtr createStagingBuffer(size_t size) = 0;
    virtual GpuImagePtr createCubeMap(const std::array<TexturePtr, 6>& textures) = 0;
    virtual GpuImagePtr createTexture(const Texture& texture) = 0;
    virtual GpuImagePtr createNormalMap(const Texture& texture) = 0;
    virtual GpuImagePtr createDepthAttachment(VkExtent2D extent) = 0;
    virtual GpuImagePtr createShadowMap(VkExtent2D extent) = 0;

    virtual void update(uint32_t frame) = 0;  // Call every frame
    virtual void writeToBuffer(GpuBuffer& buffer, const char* data, size_t size) = 0;

    virtual ~GpuBufferManager() = default;

    virtual void dbg_printMemUsageInfo(std::ostream& stream) const = 0;
};

using GpuBufferManagerPtr = std::unique_ptr<GpuBufferManager>;

GpuBufferManagerPtr createGpuBufferManager(VkPhysicalDevice physicalDevice, VkDevice device,
  VkInstance instance, VkCommandPool commandPool, VkQueue queue, std::thread::id renderThreadId,
  WorkQueue& workQueue, Logger& logger);

} // namespace render
} // namespace lithic3d
