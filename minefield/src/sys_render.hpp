#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"

const size_t DYNAMIC_TEXT_MAX_LEN = 31;

using ViewportId = size_t;

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CSprite
struct SpriteData
{
  ViewportId viewport = 0;
  Rectf textureRect;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
struct QuadData
{
  ViewportId viewport = 0;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CSprite
struct TextData
{
  ViewportId viewport = 0;
  Rectf textureRect;
  std::string text;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

// Requires components:
//   CGlobalTransform
//   CSpatialFlags
//   CRender
//   CSprite
//   CDynamicText
struct DynamicTextData
{
  ViewportId viewport = 0;
  Rectf textureRect;
  std::string text;
  size_t maxLength = DYNAMIC_TEXT_MAX_LEN;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

const size_t POLYGON_MAX_VERTICES = 8;

struct CRender
{
  Vec4f colour;
  uint32_t zIndex = 0;
  bool visible = true;
  ViewportId viewport = 0;

  static constexpr ComponentType TypeId = ComponentTypeId::CRenderTypeId;
};

struct CSprite
{
  Rectf textureRect;
  bool isText = false; // TODO: Remove?

  static constexpr ComponentType TypeId = ComponentTypeId::CSpriteTypeId;
};

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

    virtual void setClearColour(const Vec4f& colour) = 0;
    virtual void addViewport(ViewportId id, const Recti& viewport) = 0;

    virtual void addEntity(EntityId entityId, const SpriteData& data) = 0;
    virtual void addEntity(EntityId entityId, const TextData& data) = 0;
    virtual void addEntity(EntityId entityId, const DynamicTextData& data) = 0;
    virtual void addEntity(EntityId entityId, const QuadData& data) = 0;

    virtual void setZIndex(EntityId entityId, uint32_t zIndex) = 0;
    virtual void setTextureRect(EntityId entityId, const Rectf& textureRect) = 0;
    virtual void setVisible(EntityId entityId, bool visible) = 0;
    virtual void setColour(EntityId entityId, const Vec4f& colour) = 0;
    virtual void updateDynamicText(EntityId entityId, const std::string& text) = 0;

    virtual ~SysRender() {}
};

using SysRenderPtr = std::unique_ptr<SysRender>;

namespace render { class Renderer; }
class FileSystem;
class Logger;

SysRenderPtr createSysRender(ComponentStore& componentStore, render::Renderer& renderer,
  const FileSystem& fileSystem, Logger& logger);
