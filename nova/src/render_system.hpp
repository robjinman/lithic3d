#pragma once

#include "system.hpp"
#include "renderables.hpp"

struct CRender : public Component
{
  CRender(EntityId entityId)
    : Component(entityId) {}

  RenderItemId texture;
  Vec2f offset;
  Vec2f size;
};

using CRenderPtr = std::unique_ptr<CRender>;

class Camera;

class RenderSystem : public System
{
  public:
    virtual void start() = 0;
    virtual double frameRate() const = 0;

    virtual Camera& camera() = 0;
    virtual const Camera& camera() const = 0;

    CRender& getComponent(EntityId entityId) override = 0;
    const CRender& getComponent(EntityId entityId) const override = 0;

    virtual ~RenderSystem() {}
};

using RenderSystemPtr = std::unique_ptr<RenderSystem>;

class SpatialSystem;
namespace render { class Renderer; }
class FileSystem;
class Logger;

RenderSystemPtr createRenderSystem(const SpatialSystem& spatialSystem, render::Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);
