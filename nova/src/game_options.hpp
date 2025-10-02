#pragma once

#include <cstdint>

struct GameOptions
{
  uint32_t mines = 0;
  uint32_t coins = 0;
  uint32_t nuggets = 0;
  uint32_t sticks = 0;
  uint32_t wanderers = 0;
  uint32_t goldRequired = 0;
  uint32_t timeAvailable = 0;
};

const uint32_t NUM_DIFFICULTY_LEVELS = 5;

GameOptions getOptionsForLevel(uint32_t level);
