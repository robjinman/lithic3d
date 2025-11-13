#pragma once

#include "vulkan/vulkan_utils.hpp"
#include <vk_mem_alloc.h>
#include <array>
#include <memory>

namespace lithic3d
{
namespace render
{

class Ubo
{
  public:
    Ubo(VkDevice device, VmaAllocator allocator, size_t size);

    void write(const void* data, size_t size);
    VkBuffer buffer();

    ~Ubo();

  private:
    VkDevice m_device;
    VmaAllocator m_allocator;
    VmaAllocationInfo m_allocInfo;
    VmaAllocation m_alloc;
    VkBuffer m_buffer;
};

using UboPtr = std::unique_ptr<Ubo>;

class BufferedUbo
{
  public:
    BufferedUbo(VkDevice device, VmaAllocator allocator, size_t size);

    void write(size_t frame, const void* data, size_t size);
    VkBuffer buffer(size_t frame);

  private:
    std::array<Ubo, MAX_FRAMES_IN_FLIGHT> m_ubos;
};

using BufferedUboPtr = std::unique_ptr<BufferedUbo>;

} // namespace render
} // namespace lithic3d
