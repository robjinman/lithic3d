#pragma once

#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace lithic3d
{

// Queue up tasks from one thread for another thread to execute

class WorkQueue
{
  public:
    std::future<void> addWorkItem(std::function<void()>&& task)
    {
      auto packagedTask = std::make_shared<std::packaged_task<void()>>(std::move(task));
      std::future<void> future = packagedTask->get_future();

      {
        std::scoped_lock lock{m_mutex};
        m_tasks.push_back([packagedTask]() { (*packagedTask)(); });
      }

      return future;
    }

    void runAll()
    {
      std::vector<std::function<void()>> tasks;

      {
        std::scoped_lock lock{m_mutex};
        tasks = m_tasks;
        m_tasks.clear();
      }

      for (auto& task : tasks) {
        task();
      }
    }

  private:
    std::mutex m_mutex;
    std::vector<std::function<void()>> m_tasks;
};

} // namespace lithic3d
