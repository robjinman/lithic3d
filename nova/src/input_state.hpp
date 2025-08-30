#pragma once

#include "math.hpp"

struct InputState
{
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool enter = false;
  bool escape = false;
  Vec2f mousePos;
  bool mouseBtn = false;
};
