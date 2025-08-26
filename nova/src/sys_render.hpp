#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

struct CRender
{
  Rectf textureRect;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct CRenderView
{
  Rectf textureRect;
  Vec4f colour;
  uint32_t zIndex;
  bool visible;

  static constexpr ComponentType TypeId = ComponentTypeId::CRenderTypeId;
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
    virtual void setZIndex(EntityId entityId, uint32_t zIndex) = 0;
    virtual void setTextureRect(EntityId entityId, const Rectf& textureRect) = 0;
    virtual void setVisible(EntityId entityId, bool visible) = 0;

    virtual ~SysRender() {}
};

using SysRenderPtr = std::unique_ptr<SysRender>;

namespace render { class Renderer; }
class FileSystem;
class Logger;

SysRenderPtr createSysRender(ComponentStore& componentStore, render::Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);
