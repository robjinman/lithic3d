#include <lithic3d/lithic3d.hpp>
#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <wx/wx.h>

namespace
{

class VulkanWindowDelegateImpl : public lithic3d::VulkanWindowDelegate
{
  public:
    VulkanWindowDelegateImpl(WXWidget window);

    const std::vector<const char*>& getRequiredExtensions() const override;
    VkSurfaceKHR createSurface(VkInstance instance) override;
    void getFrameBufferSize(int& width, int& height) const override;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl(WXWidget window)
{
  // TODO
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  static std::vector<const char*> extensions{
    VK_KHR_SURFACE_EXTENSION_NAME,
    // TODO
  };

  return extensions;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance)
{
  VkSurfaceKHR surface;

  // TODO

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  // TODO
}

}

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window)
{
  return std::make_unique<VulkanWindowDelegateImpl>(window);
}
