#include "platform_paths.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

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
  PWSTR wpath = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL, &wpath))) {
      std::wstring base(wpath);
      CoTaskMemFree(wpath);
      m_userDataPath = fs::path(base) / "freeholdapps" / "minefield";
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

PlatformPathsPtr createPlatformPaths()
{
  return std::make_unique<PlatformPathsImpl>();
}
