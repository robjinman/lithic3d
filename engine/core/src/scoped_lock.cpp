#include "lithic3d/scoped_lock.hpp"
#include <sstream>

namespace lithic3d
{
namespace
{

void pad(std::stringstream& stream, size_t n)
{
  size_t i = stream.tellp();
  if (i < n) {
    stream << std::string(n - i, ' ');
  }
}

template<typename T>
void withPadding(std::stringstream& stream, const T& value, size_t n)
{
  stream << value;
  pad(stream, n);
}

} // namespace

LockContentionStats* LockContentionStats::m_instance = nullptr;

void LockContentionStats::printSummary(std::ostream& stream) const
{
  std::vector<Entry> entries;
  for (auto& e : m_entries) {
    entries.push_back(e.second);
  }
  std::sort(entries.begin(), entries.end(), [](const Entry& A, const Entry& B) {
    return A.time > B.time;
  });

  size_t totalTimeEnd = 16;
  size_t avrTimeEnd = 32;
  size_t nEnd = 48;
  size_t lineEnd = 64;
  size_t fileEnd = 80;

  stream << "Lock contention summary" << std::endl;

  std::stringstream ss;
  withPadding(ss, "Total time", totalTimeEnd);
  withPadding(ss, "Average time", avrTimeEnd);
  withPadding(ss, "n", nEnd);
  withPadding(ss, "Line", lineEnd);
  withPadding(ss, "File", fileEnd);

  stream << ss.str() << std::endl;
  ss.str("");

  for (auto& e : entries) {
    withPadding(ss, e.time, totalTimeEnd);
    withPadding(ss, e.time / e.n, avrTimeEnd);
    withPadding(ss, e.n, nEnd);
    withPadding(ss, e.line, lineEnd);
    withPadding(ss, e.file, fileEnd);

    stream << ss.str() << std::endl;
    ss.str("");
  }
}

} // namespace lithic3d
