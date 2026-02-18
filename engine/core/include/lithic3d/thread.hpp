#pragma once

#include "functor.hpp"
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
    std::future<T> run(Functor<T>&& task)
    {
      auto promise = std::make_shared<std::promise<T>>();

      {
        std::lock_guard lock(m_mutex);

        m_tasks.push([promise, task = std::move(task)]() mutable {
          try {
            if constexpr (std::is_same_v<T, void>) {
              task();
              promise->set_value();
            }
            else {
              promise->set_value(task());
            }
          }
          catch (...) {
            promise->set_exception(std::current_exception());
          }
        });
        m_hasWork = true;
      }

      m_conditionVariable.notify_all();

      return promise->get_future();
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
        Functor<void> task;

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
    std::queue<Functor<void>> m_tasks;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_conditionVariable;
};

} // namespace lithic3d
