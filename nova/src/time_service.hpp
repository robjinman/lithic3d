#pragma once

#include "units.hpp"
#include <memory>
#include <functional>

using Task = std::function<void()>;

class TimeService
{
  public:
    virtual void update() = 0;
    virtual void scheduleTask(Tick delay, const Task& task) = 0;
    virtual void clear() = 0;

    virtual ~TimeService() = default;
};

using TimeServicePtr = std::unique_ptr<TimeService>;

TimeServicePtr createTimeService();
