#include "utils.hpp"
#include "version.hpp"
#include <fge/platform.hpp>
#include <fge/strings.hpp>

std::string getVersionString()
{
  static std::string s = []() {
    return STR(Minefield_VERSION_MAJOR << "." << Minefield_VERSION_MINOR
      << "-" << fge::PLATFORM_NAME
#ifdef DRM
      << "-drm"
#endif
      << "");
  }();

  return s;
}
