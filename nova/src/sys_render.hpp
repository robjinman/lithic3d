#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CSprite
struct SpriteData
{
  Rectf textureRect;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CSprite
//   CDynamicText (if dynamic)
struct TextData
{
  SpriteData spriteData;
  std::string text;
};

const size_t POLYGON_MAX_VERTICES = 8;

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CPolygon
struct PolygonData
{
  std::array<Vec2f, POLYGON_MAX_VERTICES> vertices;
};

struct CRender
{
  Vec4f colour;
  uint32_t zIndex = 0;
  bool visible = true;

  static constexpr ComponentType TypeId = ComponentTypeId::CRenderTypeId;
};

struct CPolygon
{
  Vec2f vertices[POLYGON_MAX_VERTICES];

  static constexpr ComponentType TypeId = ComponentTypeId::CPolygonTypeId;
};

struct CSprite
{
  Rectf textureRect;
  bool isText = false; // TODO: Remove?

  static constexpr ComponentType TypeId = ComponentTypeId::CSpriteTypeId;
};

const size_t DYNAMIC_TEXT_MAX_LEN = 31;

struct CDynamicText
{
  char text[DYNAMIC_TEXT_MAX_LEN + 1];  // Null-terminated

  static constexpr ComponentType TypeId = ComponentTypeId::CDynamicTextTypeId;
};

class Camera;

class SysRender : public System
{
  public:
    virtual void start() = 0;
    virtual double frameRate() const = 0;

    virtual Camera& camera() = 0;
    virtual const Camera& camera() const = 0;

    virtual void addEntity(EntityId entityId, const SpriteData& data) = 0;
    virtual void addEntity(EntityId entityId, const TextData& data) = 0;
    virtual void addEntity(EntityId entityId, const PolygonData& data) = 0;

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
