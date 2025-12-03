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
      std::scoped_lock lock{m_mutex};

      auto packagedTask = std::make_shared<std::packaged_task<void()>>(std::move(task));
      std::future<void> future = packagedTask->get_future();

      m_tasks.push_back([packagedTask]() { (*packagedTask)(); });

      return future;
    }

    void runAll()
    {
      std::scoped_lock lock{m_mutex};

      for (auto& task : m_tasks) {
        task();
      }

      m_tasks.clear();
    }

  private:
    std::mutex m_mutex;
    std::vector<std::function<void()>> m_tasks;
};

} // namespace lithic3d
