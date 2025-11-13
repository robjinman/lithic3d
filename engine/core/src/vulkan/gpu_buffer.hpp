#pragma once

#include <vk_mem_alloc.h>
#include <memory>
#include <vector>

namespace lithic3d
{
namespace render
{

struct GpuBuffer
{
  GpuBuffer(VmaAllocator allocator) : allocator(allocator) {}

  VmaAllocator allocator;
  VmaAllocationInfo allocationInfo;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;

  GpuBuffer(const GpuBuffer& cpy) = delete;

  ~GpuBuffer();
};

using GpuBufferPtr = std::unique_ptr<GpuBuffer>;

GpuBufferPtr createUbo(VkDevice device, VmaAllocator allocator, size_t size);

GpuBufferPtr createVertexBuffer(VkDevice device, VmaAllocator allocator,
  const std::vector<char>& data);

GpuBufferPtr createIndexBuffer(VkDevice device, VmaAllocator allocator,
  const std::vector<char>& data);

void writeToBuffer(const GpuBuffer& buffer, const void* data, size_t size);

} // namespace render
} // namespace lithic3d
