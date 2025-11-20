#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <lithic3d/lithic3d.hpp>
#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <wx/wx.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

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
    CAMetalLayer* m_metalLayer;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl(WXWidget window)
{
  NSView* view = (NSView*)window;

  [view setWantsLayer:YES];

  m_metalLayer = [CAMetalLayer layer];
  m_metalLayer.device = MTLCreateSystemDefaultDevice();
  m_metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  m_metalLayer.framebufferOnly = YES;
  m_metalLayer.frame = view.bounds;
  m_metalLayer.contentsScale = view.window.backingScaleFactor;
  m_metalLayer.drawableSize = CGSizeMake(view.bounds.size.width * m_metalLayer.contentsScale,
    view.bounds.size.height * m_metalLayer.contentsScale);

  view.layer = m_metalLayer;
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  static std::vector<const char*> extensions{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_METAL_SURFACE_EXTENSION_NAME
  };

  return extensions;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance)
{
  VkSurfaceKHR surface{};

  VkMetalSurfaceCreateInfoEXT info{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT};
  info.pLayer = m_metalLayer;

  if (vkCreateMetalSurfaceEXT(instance, &info, nullptr, &surface) != VK_SUCCESS) {
    EXCEPTION("Failed to create surface");
  }

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  CGSize size = m_metalLayer.drawableSize;
  width = static_cast<int>(size.width);
  height = static_cast<int>(size.height);
}

}

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window)
{
  return std::make_unique<VulkanWindowDelegateImpl>(window);
}
