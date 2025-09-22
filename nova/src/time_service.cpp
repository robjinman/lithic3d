#include "time_service.hpp"
#include <list>
#include <vector>

namespace
{

class TimeServiceImpl : public TimeService
{
  public:
    void update() override;
    void scheduleTask(Tick delay, const Task& task) override;
    void clear() override;

  private:
    std::list<std::pair<Tick, Task>> m_tasks;
};

void TimeServiceImpl::clear()
{
  m_tasks.clear();
}

void TimeServiceImpl::update()
{
  // Decrement all first in case executing tasks causes to tasks to get added
  for (auto& entry : m_tasks) {
    --entry.first;
  }

  for (auto i = m_tasks.begin(); i != m_tasks.end();) {
    if (i->first == 0) {
      i->second();
      i = m_tasks.erase(i);
    }
    else {
      ++i;
    }
  }
}

void TimeServiceImpl::scheduleTask(Tick delay, const Task& task)
{
  m_tasks.push_back({ delay, task });
}

} // namespace

TimeServicePtr createTimeService()
{
  return std::make_unique<TimeServiceImpl>();
}
