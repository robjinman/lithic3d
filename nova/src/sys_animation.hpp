#pragma once

#include "system.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

constexpr size_t MAX_ANIMATIONS = 8;
constexpr size_t MAX_ANIMATION_FRAMES = 8;

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

struct CAnimation
{
  std::vector<Animation> animations;
};

// Matches layout of private CAnimationData, with only public fields visible
struct CAnimationView
{
#ifdef _WIN32
  char _padding[1928];
#else
  char _padding[1992];
#endif

  static constexpr ComponentType TypeId = ComponentTypeId::CAnimationTypeId;
};

class SysAnimation : public System
{
  public:
    virtual void addEntity(EntityId entityId, const CAnimation& data) = 0;
    virtual void playAnimation(EntityId entityId, HashedString name) = 0;
    virtual bool hasAnimationPlaying(EntityId entityId) const = 0;

    virtual ~SysAnimation() = default;
};

using SysAnimationPtr = std::unique_ptr<SysAnimation>;

class Logger;
class EventSystem;

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger);
