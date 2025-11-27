#pragma once

#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <queue>
#include <cassert>
#include <memory>

namespace lithic3d
{

class Thread
{
  public:
    Thread()
    {
      m_thread = std::thread([this]() { loop(); });
    }

    template<typename T>
    std::future<T> run(const std::function<T()>& task)
    {
      auto packagedTask = std::make_shared<std::packaged_task<T()>>(task);
      std::future<T> future = packagedTask->get_future();

      {
        std::lock_guard lock(m_mutex);

        m_tasks.push([packagedTask]() { (*packagedTask)(); });
        m_hasWork = true;
      }

      m_conditionVariable.notify_all();

      return future;
    }

    void waitAll() const
    {
      assert(std::this_thread::get_id() != m_thread.get_id());

      std::unique_lock lock(m_mutex);
      m_conditionVariable.wait(lock, [this]() { return !m_hasWork; });
    }

    std::thread::id id() const
    {
      return m_thread.get_id();
    }

    ~Thread()
    {
      waitAll();

      {
        std::lock_guard lock(m_mutex);
        m_running = false;
      }
      m_conditionVariable.notify_all();
      m_thread.join();
    }

  private:
    void loop()
    {
      while (true) {
        std::function<void()> task;

        {
          std::unique_lock lock(m_mutex);
          m_conditionVariable.wait(lock, [this]() { return m_hasWork || !m_running; });

          if (!m_running) {
            return;
          }

          assert(!m_tasks.empty());

          task = std::move(m_tasks.front());
          m_tasks.pop();
        }

        task();

        {
          std::unique_lock lock(m_mutex);
          m_hasWork = !m_tasks.empty();
          m_conditionVariable.notify_all();
        }
      }
    }

    std::thread m_thread;
    bool m_running = true;
    bool m_hasWork = false;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_conditionVariable;
};

} // namespace lithic3d
