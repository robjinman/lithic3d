#include <lithic3d/vulkan/vulkan_window_delegate.hpp>
#include <lithic3d/exception.hpp>

namespace lithic3d
{
namespace
{

class VulkanWindowDelegateImpl : public VulkanWindowDelegate
{
  public:
    VulkanWindowDelegateImpl();

    const std::vector<const char*>& getRequiredExtensions() const override;
    bool needsPhysicalDeviceForSurfaceCreation() const override;
    VkSurfaceKHR createSurface(VkInstance instance, VkPhysicalDevice physicalDevice) override;
    void getFrameBufferSize(int& width, int& height) const override;

  private:
    std::vector<const char*> m_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_DISPLAY_EXTENSION_NAME
    };
    uint32_t m_width = 1920;  // TODO
    uint32_t m_height = 1080;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl()
{
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  return m_extensions;
}

bool VulkanWindowDelegateImpl::needsPhysicalDeviceForSurfaceCreation() const
{
  return true;
}

bool findSuitableDisplayMode(VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
  VkDisplayKHR& display, VkDisplayModeKHR& displayMode)
{
  uint32_t displayPropCount = 0;

  vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropCount, NULL);
  std::vector<VkDisplayPropertiesKHR> displayProps(displayPropCount);
  vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropCount, displayProps.data());

  for (uint32_t i = 0; i < displayPropCount; ++i) {
    display = displayProps[i].display;
    uint32_t modeCount = 0;
    vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, NULL);
    std::vector<VkDisplayModePropertiesKHR> modeProps(modeCount);
    vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, modeProps.data());

    for (uint32_t j = 0; j < modeCount; ++j) {
      const auto& mode = modeProps[j];

      if (mode.parameters.visibleRegion.width == width &&
        mode.parameters.visibleRegion.height == height) {

        displayMode = mode.displayMode;
        return true;
      }
    }
  }

  return false;
}

bool findSuitablePlaneIndex(VkPhysicalDevice physicalDevice,
  const std::vector<VkDisplayPlanePropertiesKHR>& planeProps, VkDisplayKHR display, uint32_t& index)
{
  for (uint32_t i = 0; i < planeProps.size(); i++) {
    uint32_t displayCount = 0;
    vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, i, &displayCount, NULL);
    std::vector<VkDisplayKHR> displays(displayCount);
    vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, i, &displayCount, displays.data());

    for (uint32_t j = 0; j < displayCount; j++) {
      if (display == displays[j]) {
        index = i;
        return true;
      }
    }
  }

  return false;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance,
  VkPhysicalDevice physicalDevice)
{
  VkDisplayKHR display = VK_NULL_HANDLE;
  VkDisplayModeKHR displayMode = VK_NULL_HANDLE;

  if (!findSuitableDisplayMode(physicalDevice, m_width, m_height, display, displayMode)) {
    EXCEPTION("Couldn't find a display and a display mode");
  }

  uint32_t planePropCount = 0;
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropCount, NULL);
  std::vector<VkDisplayPlanePropertiesKHR> planeProps(planePropCount);
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropCount, planeProps.data());

  uint32_t planeIndex = 0;
  if (!findSuitablePlaneIndex(physicalDevice, planeProps, display, planeIndex)) {
    EXCEPTION("Couldn't find a plane for displaying");
  }

  VkDisplayPlaneCapabilitiesKHR planeCap;
  vkGetDisplayPlaneCapabilitiesKHR(physicalDevice, displayMode, planeIndex, &planeCap);
  VkDisplayPlaneAlphaFlagBitsKHR alphaMode{};

  if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
  }
  else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
  }
  else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
  }
  else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR) {
    alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
  }

  VkDisplaySurfaceCreateInfoKHR surfaceInfo{
    .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
    .pNext = NULL,
    .flags = 0,
    .displayMode = displayMode,
    .planeIndex = planeIndex,
    .planeStackIndex = planeProps[planeIndex].currentStackIndex,
    .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .globalAlpha = 1.f,
    .alphaMode = alphaMode,
    .imageExtent = {
      .width = m_width,
      .height = m_height
    }
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  auto result = vkCreateDisplayPlaneSurfaceKHR(instance, &surfaceInfo, NULL, &surface);
  if (result != VK_SUCCESS) {
    EXCEPTION("Failed to create surface, result = " << result);
  }

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{
  width = m_width;
  height = m_height;
}

}

WindowDelegatePtr createWindowDelegate()
{
  return std::make_unique<VulkanWindowDelegateImpl>();
}

} // namespace lithic3d
