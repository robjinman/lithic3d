#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace lithic3d
{
namespace render
{

class GpuBuffer
{
  public:
    virtual size_t size() const = 0;
    virtual void* mappedAddress() = 0;
    virtual VkBuffer vkBuffer() = 0;

    virtual ~GpuBuffer() = default;
};

using GpuBufferPtr = std::unique_ptr<GpuBuffer>;

class GpuBufferManager
{
  public:
    virtual GpuBufferPtr createUbo(size_t size) = 0;
    virtual GpuBufferPtr createVertexBuffer(const std::vector<char>& data) = 0;
    virtual GpuBufferPtr createIndexBuffer(const std::vector<char>& data) = 0;
    virtual GpuBufferPtr createInstanceBuffer(size_t size) = 0;
    virtual GpuBufferPtr createStagingBuffer(size_t size) = 0;

    virtual void writeToBuffer(GpuBuffer& buffer, const void* data, size_t size) = 0;

    virtual ~GpuBufferManager() = default;
};

using GpuBufferManagerPtr = std::unique_ptr<GpuBufferManager>;

GpuBufferManagerPtr createGpuBufferManager(VkPhysicalDevice physicalDevice, VkDevice device,
  VkInstance instance, VkQueue graphicsQueue, VkCommandPool commandPool);

} // namespace render
} // namespace lithic3d
