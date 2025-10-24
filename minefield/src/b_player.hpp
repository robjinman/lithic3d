#pragma once

#include <fge/sys_behaviour.hpp>

class BPlayer : public fge::BehaviourData
{
  public:
    virtual bool moveUp() = 0;
    virtual bool moveDown() = 0;
    virtual bool moveRight() = 0;
    virtual bool moveLeft() = 0;

    virtual ~BPlayer() = default;
};

using BPlayerPtr = std::unique_ptr<BPlayer>;

namespace fge
{
class Ecs;
class EventSystem;
}

BPlayerPtr createBPlayer(fge::Ecs& ecs, fge::EventSystem& eventSystem, fge::EntityId entityId);
