#include "vulkan/gpu_buffer_manager.hpp"
#include "vulkan/vulkan_utils.hpp"
#include <vk_mem_alloc.h>
#include <cstring>

namespace lithic3d
{
namespace render
{
namespace
{

class GpuBufferImpl : public GpuBuffer
{
  public:
    VmaAllocationInfo allocationInfo;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;

    GpuBufferImpl(VmaAllocator allocator);
    GpuBufferImpl(const GpuBufferImpl& cpy) = delete;

    size_t size() const override;
    void* mappedAddress() override;
    VkBuffer vkBuffer() override;

    ~GpuBufferImpl();

  private:
    VmaAllocator m_allocator;
};

GpuBufferImpl::GpuBufferImpl(VmaAllocator allocator)
  : m_allocator(allocator)
{
}

size_t GpuBufferImpl::size() const
{
  return allocationInfo.size;
}

void* GpuBufferImpl::mappedAddress()
{
  return allocationInfo.pMappedData;
}

VkBuffer GpuBufferImpl::vkBuffer()
{
  return buffer;
}

GpuBufferImpl::~GpuBufferImpl()
{
  vmaDestroyBuffer(m_allocator, buffer, allocation);
}

class GpuImageImpl : public GpuImage
{
  public:
    VmaAllocationInfo allocationInfo;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImage image;
    VkImageView imageView;

    GpuImageImpl(VmaAllocator allocator, VkDevice device);
    GpuImageImpl(const GpuImageImpl& cpy) = delete;

    VkImage vkImage() override;
    VkImageView vkImageView() override;

    ~GpuImageImpl();

  private:
    VmaAllocator m_allocator;
    VkDevice m_device;
};

GpuImageImpl::GpuImageImpl(VmaAllocator allocator, VkDevice device)
  : m_allocator(allocator)
  , m_device(device)
{
}

VkImage GpuImageImpl::vkImage()
{
  return image;
}

VkImageView GpuImageImpl::vkImageView()
{
  return imageView;
}

GpuImageImpl::~GpuImageImpl()
{
  vkDestroyImageView(m_device, imageView, nullptr);
  vmaDestroyImage(m_allocator, image, allocation);
}

class GpuBufferManagerImpl : public GpuBufferManager
{
  public:
    GpuBufferManagerImpl(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance,
      VkQueue graphicsQueue, VkCommandPool commandPool);

    GpuBufferPtr createUbo(size_t size) override;
    GpuBufferPtr createVertexBuffer(const std::vector<char>& data) override;
    GpuBufferPtr createIndexBuffer(const std::vector<char>& data) override;
    GpuBufferPtr createInstanceBuffer(size_t size) override;
    GpuBufferPtr createStagingBuffer(size_t size) override;
    GpuImagePtr createCubeMap(const std::array<TexturePtr, 6>& textures) override;

    void writeToBuffer(GpuBuffer& buffer, const void* data, size_t size) override;

    ~GpuBufferManagerImpl();

  private:
    VmaAllocator m_allocator;
    VkDevice m_device;
    VkCommandPool m_commandPool;
    VkQueue m_graphicsQueue;

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    GpuBufferPtr createDeviceBuffer(const std::vector<char>& data, VkBufferUsageFlags usage);
    GpuBufferPtr createDeviceBuffer(size_t size, VkBufferUsageFlags usage);

    void transitionImageLayout(GpuImageImpl& image, VkImageLayout oldLayout,
      VkImageLayout newLayout, uint32_t layerCount);
    void copyBufferToImage(GpuBuffer& buffer, GpuImage& image, uint32_t width, uint32_t height,
      VkDeviceSize bufferOffset, uint32_t layer);
};

GpuBufferManagerImpl::GpuBufferManagerImpl(VkPhysicalDevice physicalDevice, VkDevice device,
  VkInstance instance, VkQueue graphicsQueue, VkCommandPool commandPool)
  : m_device(device)
  , m_commandPool(commandPool)
  , m_graphicsQueue(graphicsQueue)
{
  VmaVulkanFunctions vulkanFunctions = {};
  vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo createInfo{
    .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
    .physicalDevice = physicalDevice,
    .device = device,
    .preferredLargeHeapBlockSize = 0,
    .pAllocationCallbacks = nullptr,
    .pDeviceMemoryCallbacks = nullptr,
    .pHeapSizeLimit = nullptr,
    .pVulkanFunctions = &vulkanFunctions,
    .instance = instance,
    .vulkanApiVersion = VK_API_VERSION_1_2,
    .pTypeExternalMemoryHandleTypes = nullptr
  };
  VK_CHECK(vmaCreateAllocator(&createInfo, &m_allocator),
    "Failed to create vulkan memory allocator");
}

GpuBufferPtr GpuBufferManagerImpl::createStagingBuffer(size_t size)
{
  auto buffer = std::make_unique<GpuBufferImpl>(m_allocator);

  VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = size,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr
  };

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buffer->buffer,
    &buffer->allocation, &buffer->allocationInfo), "Failed to create buffer");

  return buffer;
}

VkCommandBuffer GpuBufferManagerImpl::beginSingleTimeCommands()
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

void GpuBufferManagerImpl::endSingleTimeCommands(VkCommandBuffer commandBuffer)
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

GpuBufferPtr GpuBufferManagerImpl::createUbo(size_t size)
{
  auto buffer = std::make_unique<GpuBufferImpl>(m_allocator);

  VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = size,
    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr
  };

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buffer->buffer,
    &buffer->allocation, &buffer->allocationInfo), "Failed to create buffer");

  return buffer;
}

GpuBufferPtr GpuBufferManagerImpl::createDeviceBuffer(const std::vector<char>& data,
  VkBufferUsageFlags usage)
{
  VkDeviceSize size = data.size();

  auto stagingBufferPtr = createStagingBuffer(size);
  auto& stagingBuffer = dynamic_cast<GpuBufferImpl&>(*stagingBufferPtr);

  memcpy(stagingBuffer.mappedAddress(), data.data(), size);
  VK_CHECK(vmaFlushAllocation(m_allocator, stagingBuffer.allocation, 0, size),
    "Failed to flush memory ranges");

  auto bufferPtr = createDeviceBuffer(size, usage);
  auto& buffer = dynamic_cast<GpuBufferImpl&>(*bufferPtr);

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{
    .srcOffset = 0,
    .dstOffset = 0,
    .size = size
  };

  vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, buffer.buffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);

  return bufferPtr;
}

GpuBufferPtr GpuBufferManagerImpl::createDeviceBuffer(size_t size, VkBufferUsageFlags usage)
{
  auto buffer = std::make_unique<GpuBufferImpl>(m_allocator);

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

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buffer->buffer,
    &buffer->allocation, &buffer->allocationInfo), "Failed to create buffer");

  return buffer;
}

GpuBufferPtr GpuBufferManagerImpl::createVertexBuffer(const std::vector<char>& data)
{
  auto usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  return createDeviceBuffer(data, usage);
}

GpuBufferPtr GpuBufferManagerImpl::createIndexBuffer(const std::vector<char>& data)
{
  auto usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  return createDeviceBuffer(data, usage);
}

GpuBufferPtr GpuBufferManagerImpl::createInstanceBuffer(size_t size)
{
  auto usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  return createDeviceBuffer(size, usage);
}

void GpuBufferManagerImpl::writeToBuffer(GpuBuffer& buffer_, const void* data, size_t size)
{
  auto& buffer = dynamic_cast<GpuBufferImpl&>(buffer_);

  memcpy(buffer.allocationInfo.pMappedData, data, size);

  VK_CHECK(vmaFlushAllocation(m_allocator, buffer.allocation, 0, VK_WHOLE_SIZE),
    "Failed to flush memory ranges");
}

void GpuBufferManagerImpl::transitionImageLayout(GpuImageImpl& image, VkImageLayout oldLayout,
  VkImageLayout newLayout, uint32_t layerCount)
{
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
    .image = image.image,
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

void GpuBufferManagerImpl::copyBufferToImage(GpuBuffer& buffer, GpuImage& image, uint32_t width,
  uint32_t height, VkDeviceSize bufferOffset, uint32_t layer)
{
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

  vkCmdCopyBufferToImage(commandBuffer, buffer.vkBuffer(), image.vkImage(),
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  endSingleTimeCommands(commandBuffer);
}

GpuImagePtr GpuBufferManagerImpl::createCubeMap(const std::array<TexturePtr, 6>& textures)
{
  auto image = std::make_unique<GpuImageImpl>(m_allocator, m_device);

  VkDeviceSize imageSize = textures[0]->data.size();
  VkDeviceSize cubeMapSize = imageSize * 6;

  auto stagingBuffer = createStagingBuffer(cubeMapSize);

  uint32_t width = textures[0]->width;
  uint32_t height = textures[0]->height;

  for (size_t i = 0; i < 6; ++i) {
    ASSERT(textures[i]->data.size() % 4 == 0, "Texture data size should be multiple of 4");
    ASSERT(textures[i]->width == width, "Cube map images should have same size");
    ASSERT(textures[i]->height == height, "Cube map images should have same size");

    size_t offset = i * imageSize;
    memcpy(stagingBuffer->mappedAddress() + offset, textures[i]->data.data(),
      static_cast<size_t>(imageSize));
  }

  VkImageCreateInfo imageInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_SRGB,
    .extent = VkExtent3D{
      .width = width,
      .height = height,
      .depth = 1
    },
    .mipLevels = 1,
    .arrayLayers = 6,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  allocCreateInfo.priority = 1.0f;

  VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocCreateInfo, &image->image,
    &image->allocation, nullptr), "Failed to create image");

  transitionImageLayout(*image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);

  for (uint32_t i = 0; i < 6; ++i) {
    VkDeviceSize offset = i * imageSize;
    copyBufferToImage(*stagingBuffer, *image, width, height, offset, i);
  }

  transitionImageLayout(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

  image->imageView = createImageView(m_device, image->image, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6);

  return image;
}

GpuBufferManagerImpl::~GpuBufferManagerImpl()
{
  vmaDestroyAllocator(m_allocator);
}

} // namespace

GpuBufferManagerPtr createGpuBufferManager(VkPhysicalDevice physicalDevice, VkDevice device,
  VkInstance instance, VkQueue graphicsQueue, VkCommandPool commandPool)
{
  return std::make_unique<GpuBufferManagerImpl>(physicalDevice, device, instance, graphicsQueue,
    commandPool);
}

} // namespace render
} // namespace lithic3d
