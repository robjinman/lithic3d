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

  private:
    std::list<std::pair<Tick, Task>> m_tasks;
};

void TimeServiceImpl::update()
{
  std::vector<Task*> toExecute;

  for (auto i = m_tasks.begin(); i != m_tasks.end();) {
    if (--i->first == 0) {
      toExecute.push_back(&i->second);
      i = m_tasks.erase(i);
    }
    else {
      ++i;
    }
  }

  for (auto task : toExecute) {
    (*task)();
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
