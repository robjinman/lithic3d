#pragma once

#include "lithic3d/window_delegate.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace lithic3d
{

class VulkanWindowDelegate : public WindowDelegate
{
  public:
    virtual const std::vector<const char*>& getRequiredExtensions() const = 0;
    virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;
    virtual void getFrameBufferSize(int& width, int& height) const = 0;

    virtual ~VulkanWindowDelegate() {}
};

} // namespace lithic3d
