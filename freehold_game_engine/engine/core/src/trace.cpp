#include "fge/trace.hpp"
#include "fge/logger.hpp"
#include "fge/strings.hpp"

namespace fge
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

} // namespace fge
