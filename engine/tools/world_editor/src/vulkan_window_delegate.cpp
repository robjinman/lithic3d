#include <lithic3d/lithic3d.hpp>
#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <wx/wx.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <iostream> // TODO

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
    GdkWindow* m_gdkWindow = nullptr;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl(WXWidget window)
{
  std::cout << "Window: " << window << "\n";
  m_gdkWindow = gtk_widget_get_window(reinterpret_cast<GtkWidget*>(window));
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  static std::vector<const char*> extensions{
    VK_KHR_SURFACE_EXTENSION_NAME,
    "VK_KHR_xlib_surface"
  };

  return extensions;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance)
{
  Window xWindow = GDK_WINDOW_XID(m_gdkWindow);
  Display* xDisplay = GDK_WINDOW_XDISPLAY(m_gdkWindow);

  std::cout << "xWindow: " << xWindow << "\n";
  std::cout << "xDisplay: " << xDisplay << "\n";

  VkXlibSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  createInfo.dpy = xDisplay;
  createInfo.window = xWindow;

  VkSurfaceKHR surface;
  if (vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    EXCEPTION("Error creating Vulkan surface");
  }

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  //glfwGetFramebufferSize(&m_window, &width, &height);
  //while (width == 0 || height == 0) {
  //  glfwGetFramebufferSize(&m_window, &width, &height);
  //  glfwWaitEvents();
  //}
}

}

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window)
{
  return std::make_unique<VulkanWindowDelegateImpl>(window);
}
