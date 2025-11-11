#pragma once

#include <chrono>

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

} // namespace lithic3d
