#pragma once

#include "units.hpp"
#include <chrono>
#include <map>
#include <vector>
#include <functional>

namespace lithic3d
{

class FrameRateLimiter
{
  public:
    FrameRateLimiter(unsigned frameRate);

    void wait();

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFrameTime;
    std::chrono::microseconds m_frameDuration;
};

class Timer
{
  public:
    Timer();

    double elapsed() const;
    void reset();

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

class Scheduler
{
  public:
    void update();
    void run(Tick delay, std::function<void()>&& fn);

  private:
    Tick m_currentTick = 0;
    std::map<Tick, std::vector<std::function<void()>>> m_tasks;
};

} // namespace lithic3d
