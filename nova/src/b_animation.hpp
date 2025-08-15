#pragma once

#include "sys_behaviour.hpp"
#include "math.hpp"

struct AnimationFrame
{
  Rectf textureRect;
  Vec2f delta;
};

struct Animation
{
  HashedString name;
  Tick duration = 1;
  std::vector<AnimationFrame> frames;
};

class BAnimation : public CBehaviour
{
  public:
    virtual void addAnimation(const Animation& animation) = 0;
    virtual void playAnimation(HashedString name) = 0;
    virtual bool hasAnimationPlaying() const = 0;

    virtual ~BAnimation() {}
};

using BAnimationPtr = std::unique_ptr<BAnimation>;

class SysRender;

BAnimationPtr createBAnimation(EntityId entityId, SysRender& sysRender);
