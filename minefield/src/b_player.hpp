#pragma once

#include "sys_behaviour.hpp"

class BPlayer : public BehaviourData
{
  public:
    virtual bool moveUp() = 0;
    virtual bool moveDown() = 0;
    virtual bool moveRight() = 0;
    virtual bool moveLeft() = 0;

    virtual ~BPlayer() = default;
};

using BPlayerPtr = std::unique_ptr<BPlayer>;

class Ecs;
class EventSystem;

BPlayerPtr createBPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId);
