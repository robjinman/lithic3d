#include "android_vulkan_window_delegate.hpp"
#include <lithic3d/exception.hpp>
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <atomic>

namespace lithic3d
{

AndroidVulkanWindowDelegate::AndroidVulkanWindowDelegate(android_app& state)
  : m_state(state)
{
  m_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  m_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
  m_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
}

const std::vector<const char*>& AndroidVulkanWindowDelegate::getRequiredExtensions() const
{
  return m_extensions;
}

VkSurfaceKHR AndroidVulkanWindowDelegate::createSurface(VkInstance instance)
{
  ASSERT(m_goodWindow, "Can't create surface until window is initialised");

  VkSurfaceKHR surface{};

  VkAndroidSurfaceCreateInfoKHR info{
    .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .window = m_state.window
  };

  if (vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface) != VK_SUCCESS) {
    EXCEPTION("Failed to create surface");
  }

  return surface;
}

void AndroidVulkanWindowDelegate::getFrameBufferSize(int& width, int& height) const
{
  if (!m_goodWindow) {
    width = 128;    // Dummy values
    height = 128;
    return;
  }

  width = ANativeWindow_getWidth(m_state.window);
  height = ANativeWindow_getHeight(m_state.window);
}

void AndroidVulkanWindowDelegate::onWindowInit()
{
  m_goodWindow = true;
}

void AndroidVulkanWindowDelegate::onWindowTerm()
{
  m_goodWindow = false;
}

AndroidVulkanWindowDelegatePtr createWindowDelegate(android_app& state)
{
  return std::make_unique<AndroidVulkanWindowDelegate>(state);
}

} // namespace lithic3d
