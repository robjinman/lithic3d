#include "utils.hpp"
#include "version.hpp"
#include <fge/strings.hpp>

std::string getVersionString()
{
  return STR(Minefield_VERSION_MAJOR << "." << Minefield_VERSION_MINOR);
}
