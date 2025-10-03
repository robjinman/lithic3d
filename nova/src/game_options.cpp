#include "game_options.hpp"
#include "exception.hpp"
#include "utils.hpp"

GameOptions getOptionsForLevel(uint32_t level)
{
  switch (level) {
    case 0:
      return GameOptions{
        .mines = 0,//30,
        .coins = 10,
        .nuggets = 0,
        .sticks = 2,
        .wanderers = 0,//3,
        .goldRequired = 1,//8,
        .timeAvailable = 240
      };
    case 1:
      return GameOptions{
        .mines = 45,
        .coins = 10,
        .nuggets = 3,
        .sticks = 3,
        .wanderers = 3,
        .goldRequired = 18,
        .timeAvailable = 300
      };
    case 2:
      return GameOptions{
        .mines = 55,
        .coins = 5,
        .nuggets = 3,
        .sticks = 2,
        .wanderers = 4,
        .goldRequired = 15,
        .timeAvailable = 240
      };
    case 3:
      return GameOptions{
        .mines = 65,
        .coins = 5,
        .nuggets = 2,
        .sticks = 2,
        .wanderers = 8,
        .goldRequired = 11,
        .timeAvailable = 300
      };
    case 4:
      return GameOptions{
        .mines = 70,
        .coins = 5,
        .nuggets = 3,
        .sticks = 3,
        .wanderers = 2,
        .goldRequired = 15,
        .timeAvailable = 300
      };
    default:
      EXCEPTION(STR("Difficulty level must be between 0 and " << NUM_DIFFICULTY_LEVELS - 1));
  }
}
