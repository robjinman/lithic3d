#pragma once

#include "time.hpp"
#include <ostream>
#include <unordered_map>

namespace lithic3d
{

class LockContentionStats
{
  private:
    struct Entry
    {
      size_t n = 0;
      double time = 0.0;
      std::string file;
      int line = 0;
    };

  public:
    static LockContentionStats& get()
    {
      if (m_instance == nullptr) {
        m_instance = new LockContentionStats;
      }
      return *m_instance;
    }

    void log(long id, double seconds, const std::string& file, int line)
    {
      auto i = m_entries.find(id);
      if (i == m_entries.end()) {
        m_entries.insert({ id, Entry{
          .n = 1,
          .time = seconds,
          .file = file,
          .line = line
        }});
      }
      else {
        i->second.time += seconds;
        ++i->second.n;
      }
    }

    void printSummary(std::ostream& stream) const;

  private:
    static LockContentionStats* m_instance;
    std::unordered_map<size_t, Entry> m_entries;
};

#ifndef NDEBUG
#define L3D_S1(x) #x
#define L3D_S2(x) L3D_S1(x)
#define L3D_LOCATION __FILE__ L3D_S2(__LINE__)
#define SCOPED_LOCK(ARG_mutex) \
  Timer MAC_SL_timer; \
  std::scoped_lock MAC_SL_lock(ARG_mutex); \
  size_t MAC_SL_lockId = std::hash<std::string>{}(L3D_LOCATION); \
  LockContentionStats::get().log(MAC_SL_lockId, MAC_SL_timer.elapsed(), __FILE__, __LINE__);
#else
#define SCOPED_LOCK(ARG_mutex) std::scoped_lock MAC_SL_lock{ARG_mutex};
#endif

}
