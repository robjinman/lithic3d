#include <lithic3d/platform_paths.hpp>

namespace fs = std::filesystem;

namespace lithic3d
{
namespace
{

class PlatformPathsImpl : public PlatformPaths
{
  public:
    PlatformPathsImpl(const std::string& appName, const std::string& vendorName);

    const std::filesystem::path& appData() const override;
    const std::filesystem::path& userData() const override;

  private:
    fs::path m_appDataPath;
    fs::path m_userDataPath;

    void determineAppDataPath();
    void determineUserDataPath(const std::string& appName, const std::string& vendorName);
};

PlatformPathsImpl::PlatformPathsImpl(const std::string& appName, const std::string& vendorName)
{
  determineAppDataPath();
  determineUserDataPath(appName, vendorName);
}

void PlatformPathsImpl::determineAppDataPath()
{
  m_appDataPath = std::filesystem::current_path() / "data";
}

void PlatformPathsImpl::determineUserDataPath(const std::string& appName,
  const std::string& vendorName)
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

} // namespace

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName)
{
  return std::make_unique<PlatformPathsImpl>(appName, vendorName);
}

} // namespace lithic3d
