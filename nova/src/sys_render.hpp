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
  uint32_t zIndex;
#ifdef _WIN32
  uint32_t padding[20];
#else
  uint32_t padding[23];
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
    virtual const Vec2f& getPosition(EntityId entityId) const = 0;
    virtual void setPosition(EntityId entityId, const Vec2f& pos) = 0;
    virtual void move(EntityId entityId, const Vec2f& delta) = 0;
    virtual void setTextureRect(EntityId entityId, const Rectf& textureRect) = 0;

    virtual ~SysRender() {}
};

using SysRenderPtr = std::unique_ptr<SysRender>;

class World;
namespace render { class Renderer; }
class FileSystem;
class Logger;

SysRenderPtr createSysRender(World& world, render::Renderer& renderer, const FileSystem& fileSystem,
  Logger& logger);
