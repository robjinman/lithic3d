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
    VkSurfaceKHR createSurface(VkInstance instance) override;
    void getFrameBufferSize(int& width, int& height) const override;

  private:
    std::vector<const char*> m_extensions;
};

VulkanWindowDelegateImpl::VulkanWindowDelegateImpl()
{
}

const std::vector<const char*>& VulkanWindowDelegateImpl::getRequiredExtensions() const
{
  return m_extensions;
}

VkSurfaceKHR VulkanWindowDelegateImpl::createSurface(VkInstance instance)
{
  VkSurfaceKHR surface;

  return surface;
}

void VulkanWindowDelegateImpl::getFrameBufferSize(int& width, int& height) const
{

}

}

WindowDelegatePtr createWindowDelegate()
{
  return std::make_unique<VulkanWindowDelegateImpl>();
}

} // namespace lithic3d
