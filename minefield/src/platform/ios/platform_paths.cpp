#include "platform_paths.hpp"

namespace fs = std::filesystem;

namespace
{

const std::string vendorName = "freeholdapps";
const std::string appName = "minefield";

class PlatformPathsImpl : public PlatformPaths
{
  public:
    PlatformPathsImpl(const std::filesystem::path& bundlePath);

    const std::filesystem::path& appData() const override;
    const std::filesystem::path& userData() const override;

  private:
    fs::path m_appDataPath;
    fs::path m_userDataPath;

    void determineAppDataPath(const std::filesystem::path& bundlePath);
    void determineUserDataPath();
};

PlatformPathsImpl::PlatformPathsImpl(const std::filesystem::path& bundlePath)
{
  determineAppDataPath(bundlePath);
  determineUserDataPath();
}

void PlatformPathsImpl::determineAppDataPath(const std::filesystem::path& bundlePath)
{
  m_appDataPath = bundlePath / "data";
}

void PlatformPathsImpl::determineUserDataPath()
{
  const char* home = std::getenv("HOME");
  fs::path base = home ? fs::path(home) : fs::path(".");
  m_userDataPath = base / ".local" / "share" / vendorName / appName;
}

const std::filesystem::path& PlatformPathsImpl::appData() const
{
  return m_appDataPath;
}

const std::filesystem::path& PlatformPathsImpl::userData() const
{
  return m_userDataPath;
}

}

PlatformPathsPtr createPlatformPaths(const std::filesystem::path& bundlePath)
{
  return std::make_unique<PlatformPathsImpl>(bundlePath);
}
