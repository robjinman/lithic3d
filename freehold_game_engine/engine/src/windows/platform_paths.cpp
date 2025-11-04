#include <fge/platform_paths.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace fge
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
  PWSTR wpath = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL, &wpath))) {
      std::wstring base(wpath);
      CoTaskMemFree(wpath);
      m_userDataPath = fs::path(base) / vendorName / appName;
  }
  else {
    char* local = std::getenv("LOCALAPPDATA");
    if (local) {
      m_userDataPath = fs::path(local) / vendorName / appName;
    }
  }
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

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName)
{
  return std::make_unique<PlatformPathsImpl>(appName, vendorName);
}

} // namespace fge
