#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

struct AnimationFrame
{
  Vec2f pos;
  Vec2f scale{ 1.f, 1.f };
  std::optional<Rectf> textureRect;
  std::optional<Vec4f> colour;
};

struct Animation
{
  HashedString name;
  Tick duration = 1;
  std::vector<AnimationFrame> frames;
};

using AnimationPtr = std::unique_ptr<Animation>;

using AnimationId = size_t;

struct CAnimation
{
  std::set<AnimationId> animations;
};

class SysAnimation : public System
{
  public:
    virtual void addEntity(EntityId entityId, const CAnimation& data) = 0;
    virtual AnimationId addAnimation(AnimationPtr animation) = 0;
    virtual void playAnimation(EntityId entityId, HashedString name, bool repeat = false) = 0;
    virtual void seek(EntityId entityId, Tick tick) = 0;
    virtual bool hasAnimationPlaying(EntityId entityId) const = 0;

    virtual ~SysAnimation() = default;
};

using SysAnimationPtr = std::unique_ptr<SysAnimation>;

class Logger;
class EventSystem;

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger);
