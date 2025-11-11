#include "lithic3d/trace.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/strings.hpp"

namespace lithic3d
{

Trace::Trace(Logger& logger, const std::string& file, const std::string& func)
  : m_logger(logger)
  , m_file(file)
  , m_func(func)
{
  m_logger.debug(STR("ENTER " << m_func << " (" << m_file << ")"));
}

Trace::~Trace()
{
  m_logger.debug(STR("EXIT " << m_func << " (" << m_file << ")"));
}

} // namespace lithic3d
