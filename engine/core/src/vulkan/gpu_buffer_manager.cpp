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
    VmaAllocator allocator;
    VmaAllocationInfo allocationInfo;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;

    GpuBufferImpl(VmaAllocator allocator);
    GpuBufferImpl(const GpuBufferImpl& cpy) = delete;

    size_t size() const override;
    void* mappedAddress() override;
    VkBuffer vkBuffer() override;

    ~GpuBufferImpl();
};

GpuBufferImpl::GpuBufferImpl(VmaAllocator allocator)
  : allocator(allocator)
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
  vmaDestroyBuffer(allocator, buffer, allocation);
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

    void writeToBuffer(GpuBuffer& buffer, const void* data, size_t size) override;

    ~GpuBufferManagerImpl();

  private:
    VmaAllocator m_allocator;
    VkDevice m_device;
    VkCommandPool m_commandPool;
    VkQueue m_graphicsQueue;

    GpuBufferPtr createStagingBuffer(size_t size);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    GpuBufferPtr createDeviceBuffer(const std::vector<char>& data, VkBufferUsageFlags usage);
    GpuBufferPtr createDeviceBuffer(size_t size, VkBufferUsageFlags usage);
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

  VK_CHECK(vmaFlushAllocation(buffer.allocator, buffer.allocation, 0, VK_WHOLE_SIZE),
    "Failed to flush memory ranges");
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
