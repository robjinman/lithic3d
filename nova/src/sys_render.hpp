#pragma once

#include "system.hpp"
#include "utils.hpp"
#include "math.hpp"

struct Animation
{
  HashedString name;
  std::vector<Recti> frames;
};

struct CRender
{
  Rectf textureRect;
  Vec2f size;
  Vec2f pos;
  uint32_t zIndex = 0;
  std::vector<Animation> animations;
};

class Camera;

class SysRender : public System
{
  public:
    virtual void start() = 0;
    virtual double frameRate() const = 0;

    virtual Camera& camera() = 0;
    virtual const Camera& camera() const = 0;

    virtual void addEntity(EntityId entityId, const CRender& data) = 0;
    virtual void moveEntity(EntityId entityId, const Vec2f& pos) = 0;
    virtual void playAnimation(EntityId entityId, HashedString name) = 0;
    virtual bool isAnimationPlaying(EntityId entityId) const = 0;

    virtual ~SysRender() {}
};

using SysRenderPtr = std::unique_ptr<SysRender>;

namespace render { class Renderer; }
class FileSystem;
class Logger;

SysRenderPtr createSysRender(render::Renderer& renderer, const FileSystem& fileSystem,
  Logger& logger);
