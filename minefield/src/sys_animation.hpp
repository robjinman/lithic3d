#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"
#include <functional>

struct AnimationFrame
{
  Vec2f pos{ 0.f, 0.f };
  float_t rotation = 0.f;
  Vec2f pivot{ 0.f, 0.f };  // In model space, which for sprites ranges from 0.0 to 1.0 on both axis
  Vec2f scale{ 1.f, 1.f };
  std::optional<Rectf> textureRect;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct Animation
{
  HashedString name = 0;
  Tick duration = 1;
  Vec2f startPos{ 0.f, 0.f };
  std::vector<AnimationFrame> frames;
};

using AnimationPtr = std::unique_ptr<Animation>;

using AnimationId = size_t;

// Requires components:
//   CSpatialFlags
//   CLocalTransform
//   CRender
struct AnimationData
{
  std::set<AnimationId> animations;
};

class SysAnimation : public System
{
  public:
    virtual void addEntity(EntityId entityId, const AnimationData& data) = 0;
    virtual AnimationId addAnimation(AnimationPtr animation) = 0;
    virtual void replaceAnimation(AnimationId animationId, AnimationPtr animation) = 0;

    // Throws exception if an animation is already playing
    virtual void playAnimation(EntityId entityId, HashedString name, bool repeat = false) = 0;
    virtual void playAnimation(EntityId entityId, HashedString name,
      const std::function<void()>& onFinish) = 0;

    // Queue an animation to play immediately following the current animation or play immediately
    // if there's none playing. Replaces any currently queued animation.
    virtual void queueAnimation(EntityId entityId, HashedString name, bool repeat = false) = 0;
    virtual void queueAnimation(EntityId entityId, HashedString name,
      const std::function<void()>& onFinish) = 0;

    // Stops the current animation and starts any queued animation.
    // WARNING: This triggers the onFinish callback, which could in turn start a new animation,
    // so don't assume no animation is playing after calling this.
    virtual void stopAnimation(EntityId entityId) = 0;

    virtual void seek(EntityId entityId, Tick tick) = 0;
    virtual bool hasAnimation(EntityId entityId, HashedString name) const = 0;
    virtual bool hasAnimationPlaying(EntityId entityId) const = 0;

    // Finishes the current animation and starts any queued animation
    virtual void finishAnimation(EntityId entityId) = 0;

    virtual ~SysAnimation() = default;
};

using SysAnimationPtr = std::unique_ptr<SysAnimation>;

class Logger;

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, Logger& logger);
