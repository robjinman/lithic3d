#pragma once

#include <stdexcept>
#include <string>
#include <sstream>

namespace fge
{

class Exception : public std::runtime_error
{
public:
  Exception(const std::string& msg, const std::string& file, int line)
    : runtime_error(msg + " (file: " + file + ", line: " + std::to_string(line) + ")") {}
};

} // namespace fge

#define EXCEPTION(msg) \
  { \
    std::stringstream MAC_ss; \
    MAC_ss << msg; \
    throw fge::Exception(MAC_ss.str(), __FILE__, __LINE__); \
  }

#define ASSERT(MAC_cond, MAC_msg) \
  if (!(MAC_cond)) { \
    EXCEPTION(MAC_msg); \
  }

#ifndef NDEBUG
#define DBG_ASSERT(MAC_cond, MAC_msg) ASSERT(MAC_cond, MAC_msg)
#else
#define DBG_ASSERT(MAC_cond, MAC_msg)
#endif
