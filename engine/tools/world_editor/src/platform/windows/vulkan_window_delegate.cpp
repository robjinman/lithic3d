#include <wx/wx.h>
#define VK_USE_PLATFORM_WIN32_KHR 1
#define VK_KHR_win32_surface 1
#include <lithic3d/lithic3d.hpp>
#include <lithic3d/vulkan/vulkan_window_delegate.hpp>

namespace
{

class VulkanWindowDelegateImpl : public lithic3d::VulkanWindowDelegate
{
  public:
    VulkanWindowDelegateImpl(WXWidget window);

    const std::vector<const char*>& getRequiredExtensions() const override;
    VkSurfaceKHR createSurface(VkInstance instance) override;
    void getFrameBufferSize(int& width, int& height) const override;

  private:
    HWND m_hwnd;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl(WXWidget window)
{
  m_hwnd = window;
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  static std::vector<const char*> extensions{
    VK_KHR_SURFACE_EXTENSION_NAME,
    "VK_KHR_win32_surface"
  };

  return extensions;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance)
{
  VkWin32SurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hwnd = m_hwnd;
  createInfo.hinstance = GetModuleHandle(NULL);

  VkSurfaceKHR surface;
  if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    EXCEPTION("Error creating Vulkan surface");
  }

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  RECT rect;
  ASSERT(GetWindowRect(m_hwnd, &rect), "Error getting window size");

  width = rect.right - rect.left;
  height = rect.bottom - rect.top;
}

}

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window)
{
  return std::make_unique<VulkanWindowDelegateImpl>(window);
}
