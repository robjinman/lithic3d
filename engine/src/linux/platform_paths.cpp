#include <fge/platform_paths.hpp>

namespace fs = std::filesystem;

namespace fge
{
namespace
{

const std::string vendorName = "freeholdapps";
const std::string appName = "minefield";

class PlatformPathsImpl : public PlatformPaths
{
  public:
    PlatformPathsImpl();

    const std::filesystem::path& appData() const override;
    const std::filesystem::path& userData() const override;

  private:
    fs::path m_appDataPath;
    fs::path m_userDataPath;

    void determineAppDataPath();
    void determineUserDataPath();
};

PlatformPathsImpl::PlatformPathsImpl()
{
  determineAppDataPath();
  determineUserDataPath();
}

void PlatformPathsImpl::determineAppDataPath()
{
  m_appDataPath = std::filesystem::current_path() / "data";
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

} // namespace

PlatformPathsPtr createPlatformPaths()
{
  return std::make_unique<PlatformPathsImpl>();
}

} // namespace fge
