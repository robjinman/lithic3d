#pragma once

#include <cstdint>
#include <memory>

struct GameOptions
{
  uint32_t mines = 0;
  uint32_t coins = 0;
  uint32_t nuggets = 0;
  uint32_t sticks = 0;
  uint32_t wanderers = 0;
  uint32_t goldRequired = 0;
  uint32_t timeAvailable = 0;
  uint32_t bestTime = 0;
};

const uint32_t NUM_DIFFICULTY_LEVELS = 5;

class GameOptionsManager
{
  public:
    virtual const GameOptions& getOptionsForLevel(uint32_t level) const = 0;
    virtual void setBestTime(uint32_t level, uint32_t time) = 0;

    virtual ~GameOptionsManager() = default;
};

using GameOptionsManagerPtr = std::unique_ptr<GameOptionsManager>;

namespace fge
{
class FileSystem;
class Logger;
}

GameOptionsManagerPtr createGameOptionsManager(fge::FileSystem& fileSystem, fge::Logger& logger);
