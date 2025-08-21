#pragma once

#include "system.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

struct CRender
{
  Rectf textureRect;
  Vec2f size;
  Vec2f pos;
  uint32_t zIndex = 0;

  static constexpr ComponentType TypeId = ComponentTypeId::CRenderTypeId;
};

// Matches layout of private CRenderData, with only public fields visible
struct CRenderView
{
  Vec2f pos;
  Rectf textureRect;
  Vec4f colour;
  uint32_t zIndex;
#ifdef _WIN32
  char _padding[80];
#else
  char _padding[92];
#endif

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

    virtual ~SysRender() {}
};

using SysRenderPtr = std::unique_ptr<SysRender>;

namespace render { class Renderer; }
class FileSystem;
class Logger;

SysRenderPtr createSysRender(ComponentStore& componentStore, render::Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);
