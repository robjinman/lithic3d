#pragma once

#include "system.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

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

using AnimationPtr = std::unique_ptr<Animation>;

struct CAnimation
{
  std::map<HashedString, AnimationPtr> animations;
};

// Matches layout of private CAnimationData, with only public fields visible
struct CAnimationView
{
#ifdef _WIN32
  uint32_t _padding[20];
#else
  uint32_t _padding[23];
#endif

  static constexpr ComponentType TypeId = ComponentTypeId::CAnimationTypeId;
};

class SysAnimation : public System
{
  public:
    virtual void addEntity(EntityId entityId, const CAnimation& data) = 0;

    virtual ~SysAnimation() {}
};

using SysAnimationPtr = std::unique_ptr<SysAnimation>;

class World;
namespace render { class Renderer; }
class FileSystem;
class Logger;

SysAnimationPtr createSysAnimation(World& world, render::Renderer& renderer, const FileSystem& fileSystem,
  Logger& logger);
