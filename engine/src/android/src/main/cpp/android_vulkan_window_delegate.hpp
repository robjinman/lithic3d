#pragma once

#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <vulkan/vulkan_android.h>
#include <atomic>
#include <vector>

struct android_app;

namespace lithic3d
{

class AndroidVulkanWindowDelegate : public VulkanWindowDelegate
{
  public:
    AndroidVulkanWindowDelegate(android_app& state);

    const std::vector<const char*>& getRequiredExtensions() const override;
    VkSurfaceKHR createSurface(VkInstance instance) override;
    void getFrameBufferSize(int& width, int& height) const override;

    void onWindowInit();
    void onWindowTerm();

  private:
    android_app& m_state;
    std::vector<const char*> m_extensions;
    std::atomic<bool> m_goodWindow = false;
};

using AndroidVulkanWindowDelegatePtr = std::unique_ptr<AndroidVulkanWindowDelegate>;

AndroidVulkanWindowDelegatePtr createWindowDelegate(android_app& state);

} // namespace lithic3d
