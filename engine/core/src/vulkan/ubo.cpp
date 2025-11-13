#include "vulkan/ubo.hpp"
#include <cstring>

namespace lithic3d
{
namespace render
{

Ubo::Ubo(VkDevice device, VmaAllocator allocator, size_t size)
  : m_device(device)
  , m_allocator(allocator)
{
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

  VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &m_buffer, &m_alloc, &m_allocInfo),
    "Failed to create buffer");
}

VkBuffer Ubo::buffer()
{
  return m_buffer;
}

void Ubo::write(const void* data, size_t size)
{
  memcpy(m_allocInfo.pMappedData, data, size);

  VK_CHECK(vmaFlushAllocation(m_allocator, m_alloc, 0, VK_WHOLE_SIZE),
    "Failed to flush memory ranges");
}

Ubo::~Ubo()
{
  vmaDestroyBuffer(m_allocator, m_buffer, m_alloc);
}

BufferedUbo::BufferedUbo(VkDevice device, VmaAllocator allocator, size_t size)
  : m_ubos{
    Ubo{device, allocator, size},
    Ubo{device, allocator, size}
  }
{
}

void BufferedUbo::write(size_t frame, const void* data, size_t size)
{
  m_ubos[frame].write(data, size);
}

VkBuffer BufferedUbo::buffer(size_t frame)
{
  return m_ubos[frame].buffer();
}

} // namespace render
} // namespace lithic3d
