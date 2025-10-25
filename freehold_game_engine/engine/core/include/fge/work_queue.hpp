#pragma once

#include "strings.hpp"
#include <future>
#include <memory>
#include <chrono>
#include <queue>

namespace fge
{

struct WorkItem
{
  WorkItem(HashedString name)
    : name(name) {}

  HashedString name;
  virtual ~WorkItem() = default;
};

using WorkItemPtr = std::unique_ptr<WorkItem>;

struct WorkItemResultValue
{
  virtual ~WorkItemResultValue() = default;
};

using WorkItemResultValuePtr = std::unique_ptr<WorkItemResultValue>;

class WorkItemResult
{
  public:
    explicit WorkItemResult(std::future<WorkItemResultValuePtr>&& future)
      : m_future(std::move(future)) {}

    template<typename T>
    const T& value()
    {
      m_result = m_future.get();
      return dynamic_cast<const T&>(*m_result);
    }

  private:
    std::future<WorkItemResultValuePtr> m_future;
    WorkItemResultValuePtr m_result = nullptr;
};

using WorkItemHandler = std::function<void(WorkItem&, std::promise<WorkItemResultValuePtr>&)>;

class WorkQueue
{
  public:
    WorkItemResult addWorkItem(WorkItemPtr item)
    {
      std::scoped_lock lock{m_mutex};

      std::promise<WorkItemResultValuePtr> promise;
      m_workItems.push(std::make_pair(std::move(item), std::move(promise)));

      return WorkItemResult{m_workItems.back().second.get_future()};
    }

    bool processNext(const WorkItemHandler& handler)
    {
      bool empty = false;
      std::pair<WorkItemPtr, std::promise<WorkItemResultValuePtr>> item;

      {
        std::scoped_lock lock{m_mutex};

        if (m_workItems.empty()) {
          return false;
        }

        item = std::move(m_workItems.front());
        m_workItems.pop();

        empty = m_workItems.empty();
      }

      handler(*item.first, item.second);

      return !empty;
    }

  private:
    std::mutex m_mutex;
    std::queue<std::pair<WorkItemPtr, std::promise<WorkItemResultValuePtr>>> m_workItems;
};

} // namespace fge
