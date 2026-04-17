#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <lithic3d/exception.hpp>
#include <vulkan/vulkan_android.h>
#include <android_native_app_glue.h>
#include <android/native_window.h>

namespace lithic3d
{
namespace
{

class AndroidWindowDelegateImpl : public VulkanWindowDelegate
{
  public:
    AndroidWindowDelegateImpl(android_app& state);

    const std::vector<const char*>& getRequiredExtensions() const override;
    VkSurfaceKHR createSurface(VkInstance instance) override;
    void getFrameBufferSize(int& width, int& height) const override;

  private:
    android_app& m_state;
    std::vector<const char*> m_extensions;
};

AndroidWindowDelegateImpl::AndroidWindowDelegateImpl(android_app& state)
  : m_state(state)
{
  m_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  m_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
  m_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
}

const std::vector<const char*>& AndroidWindowDelegateImpl::getRequiredExtensions() const
{
  return m_extensions;
}

VkSurfaceKHR AndroidWindowDelegateImpl::createSurface(VkInstance instance)
{
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

void AndroidWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  width = ANativeWindow_getWidth(m_state.window);
  height = ANativeWindow_getHeight(m_state.window);
}

}

WindowDelegatePtr createWindowDelegate(android_app& state)
{
  return std::make_unique<AndroidWindowDelegateImpl>(state);
}

} // namespace lithic3d
