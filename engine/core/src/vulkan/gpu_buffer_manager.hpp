#pragma once

#include "lithic3d/renderables.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

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

class GpuImage
{
  public:
    virtual VkImage vkImage() = 0;
    virtual VkImageView vkImageView() = 0;

    virtual ~GpuImage() = default;
};

using GpuImagePtr = std::unique_ptr<GpuImage>;

class GpuBufferManager
{
  public:
    virtual GpuBufferPtr createUbo(size_t size) = 0;
    virtual GpuBufferPtr createVertexBuffer(const std::vector<char>& data) = 0;
    virtual GpuBufferPtr createIndexBuffer(const std::vector<char>& data) = 0;
    virtual GpuBufferPtr createInstanceBuffer(size_t size) = 0;
    virtual GpuBufferPtr createStagingBuffer(size_t size) = 0;
    virtual GpuImagePtr createCubeMap(const std::array<TexturePtr, 6>& textures) = 0;
    virtual GpuImagePtr createTexture(const Texture& texture) = 0;
    virtual GpuImagePtr createNormalMap(const Texture& texture) = 0;
    virtual GpuImagePtr createDepthAttachment(VkExtent2D extent) = 0;
    virtual GpuImagePtr createShadowMap(VkExtent2D extent) = 0;

    virtual void writeToBuffer(GpuBuffer& buffer, const void* data, size_t size) = 0;

    virtual ~GpuBufferManager() = default;
};

using GpuBufferManagerPtr = std::unique_ptr<GpuBufferManager>;

GpuBufferManagerPtr createGpuBufferManager(VkPhysicalDevice physicalDevice, VkDevice device,
  VkInstance instance, VkQueue graphicsQueue, VkCommandPool commandPool);

} // namespace render
} // namespace lithic3d
