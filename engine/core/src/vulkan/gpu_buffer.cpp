#include "vulkan/gpu_buffer.hpp"
#include "vulkan/vulkan_utils.hpp"
#include <cstring>

namespace lithic3d
{
namespace render
{

GpuBufferPtr createUbo(VkDevice device, VmaAllocator allocator, size_t size)
{
  auto buffer = std::make_unique<GpuBuffer>(allocator);

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
  allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer->buffer,
    &buffer->allocation, &buffer->allocationInfo), "Failed to create buffer");

  return buffer;
}

GpuBufferPtr createVertexBuffer(VkDevice device, VmaAllocator allocator,
  const std::vector<char>& data)
{
  // TODO
}

GpuBufferPtr createIndexBuffer(VkDevice device, VmaAllocator allocator,
  const std::vector<char>& data)
{
  // TODO
}

void writeToBuffer(const GpuBuffer& buffer, const void* data, size_t size)
{
  memcpy(buffer.allocationInfo.pMappedData, data, size);

  VK_CHECK(vmaFlushAllocation(buffer.allocator, buffer.allocation, 0, VK_WHOLE_SIZE),
    "Failed to flush memory ranges");
}

GpuBuffer::~GpuBuffer()
{
  vmaDestroyBuffer(allocator, buffer, allocation);
}

} // namespace render
} // namespace lithic3d
