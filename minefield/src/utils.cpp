#include "utils.hpp"
#include "version.hpp"
#include <fge/platform.hpp>
#include <fge/strings.hpp>

std::string getVersionString()
{
  static std::string versionString = []() {
    std::stringstream ss;
    ss << Minefield_VERSION_MAJOR << "." << Minefield_VERSION_MINOR
      << "-" << fge::PLATFORM_NAME;
#ifdef DRM
    ss << "-drm";
#endif
    ss << "";
    return ss.str();
  }();

  return versionString;
}
