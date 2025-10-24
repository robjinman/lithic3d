#include "game_options.hpp"
#include <fge/exception.hpp>
#include <fge/file_system.hpp>
#include <fge/utils.hpp>
#include <cstring>

namespace
{

const std::string saveFile = "save.dat";

const std::array<GameOptions, NUM_DIFFICULTY_LEVELS> levels{
  GameOptions{
    .mines = 30,
    .coins = 10,
    .nuggets = 0,
    .sticks = 2,
    .wanderers = 3,
    .goldRequired = 8,
    .timeAvailable = 240,
    .bestTime = std::numeric_limits<uint32_t>::max()
  }, {
    .mines = 45,
    .coins = 10,
    .nuggets = 3,
    .sticks = 3,
    .wanderers = 3,
    .goldRequired = 18,
    .timeAvailable = 300,
    .bestTime = std::numeric_limits<uint32_t>::max()
  }, {
    .mines = 55,
    .coins = 5,
    .nuggets = 3,
    .sticks = 2,
    .wanderers = 4,
    .goldRequired = 15,
    .timeAvailable = 240,
    .bestTime = std::numeric_limits<uint32_t>::max()
  }, {
    .mines = 65,
    .coins = 5,
    .nuggets = 2,
    .sticks = 2,
    .wanderers = 8,
    .goldRequired = 11,
    .timeAvailable = 300,
    .bestTime = std::numeric_limits<uint32_t>::max()
  }, {
    .mines = 70,
    .coins = 5,
    .nuggets = 3,
    .sticks = 3,
    .wanderers = 2,
    .goldRequired = 15,
    .timeAvailable = 300,
    .bestTime = std::numeric_limits<uint32_t>::max()
  }
};

class GameOptionsManagerImpl : public GameOptionsManager
{
  public:
    GameOptionsManagerImpl(fge::FileSystem& fileSystem, fge::Logger& logger);

    const GameOptions& getOptionsForLevel(uint32_t level) const override;
    void setBestTime(uint32_t level, uint32_t time) override;

  private:
    fge::Logger& m_logger;
    fge::FileSystem& m_fileSystem;
    std::array<GameOptions, NUM_DIFFICULTY_LEVELS> m_levels;
};

GameOptionsManagerImpl::GameOptionsManagerImpl(fge::FileSystem& fileSystem, fge::Logger& logger)
  : m_logger(logger)
  , m_fileSystem(fileSystem)
{
  m_levels = levels;

  if (m_fileSystem.userDataFileExists(saveFile)) {
    auto bytes = m_fileSystem.readUserDataFile(saveFile);
    std::array<uint32_t, NUM_DIFFICULTY_LEVELS> times;
    ASSERT(bytes.size() == times.size() * sizeof(uint32_t), "Bad save file");
    memcpy(times.data(), bytes.data(), bytes.size());

    for (size_t i = 0; i < m_levels.size(); ++i) {
      m_levels[i].bestTime = times[i];
    }
  }
}

const GameOptions& GameOptionsManagerImpl::getOptionsForLevel(uint32_t level) const
{
  ASSERT(level < NUM_DIFFICULTY_LEVELS, "Level out of range");
  return m_levels[level];
}

void GameOptionsManagerImpl::setBestTime(uint32_t level, uint32_t time)
{
  ASSERT(level < NUM_DIFFICULTY_LEVELS, "Level out of range");
  m_levels[level].bestTime = time;

  std::array<uint32_t, NUM_DIFFICULTY_LEVELS> times;
  for (size_t i = 0; i < m_levels.size(); ++i) {
    times[i] = m_levels[i].bestTime;
  }

  m_fileSystem.writeUserDataFile(saveFile, reinterpret_cast<const char*>(times.data()),
    times.size() * sizeof(uint32_t));
}

}

GameOptionsManagerPtr createGameOptionsManager(fge::FileSystem& fileSystem, fge::Logger& logger)
{
  return std::make_unique<GameOptionsManagerImpl>(fileSystem, logger);
}
