#include "platform_paths.hpp"

namespace fs = std::filesystem;

namespace fge
{
namespace
{

class PlatformPathsImpl : public PlatformPaths
{
  public:
    PlatformPathsImpl(const fs::path& bundlePath, const fs::path& appSupportPath);

    const fs::path& appData() const override;
    const fs::path& userData() const override;

  private:
    fs::path m_appDataPath;
    fs::path m_userDataPath;

    void determineAppDataPath(const fs::path& bundlePath);
    void determineUserDataPath(const fs::path& bundlePath);
};

PlatformPathsImpl::PlatformPathsImpl(const fs::path& bundlePath, const fs::path& appSupportPath)
{
  determineAppDataPath(bundlePath);
  determineUserDataPath(appSupportPath);
}

void PlatformPathsImpl::determineAppDataPath(const fs::path& bundlePath)
{
  m_appDataPath = bundlePath / "data";
}

void PlatformPathsImpl::determineUserDataPath(const fs::path& appSupportPath)
{
  m_userDataPath = appSupportPath;
}

const fs::path& PlatformPathsImpl::appData() const
{
  return m_appDataPath;
}

const fs::path& PlatformPathsImpl::userData() const
{
  return m_userDataPath;
}

}

PlatformPathsPtr createPlatformPaths(const fs::path& bundlePath, const fs::path& appSupportPath)
{
  return std::make_unique<PlatformPathsImpl>(bundlePath, appSupportPath);
}

} // namespace fge
