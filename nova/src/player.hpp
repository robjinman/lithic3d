#pragma once

#include <memory>

class Player
{
  public:
    virtual ~Player() {}
};

using PlayerPtr = std::unique_ptr<Player>;
