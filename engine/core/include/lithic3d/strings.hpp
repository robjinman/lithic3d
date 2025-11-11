#pragma once

#include <string>
#include <sstream>

namespace lithic3d
{

#define STR(x) [&]() {\
  std::stringstream MAC_ss; \
  MAC_ss << x; \
  return MAC_ss.str(); \
}()

using HashedString = size_t;

HashedString hashString(const std::string& s);
std::string getHashedString(HashedString hash);

} // namespace lithic3d
