#pragma once

#include "ecs.hpp"
#include "utils.hpp"
#include "math.hpp"
#include "component_types.hpp"
#include "systems.hpp"
#include "renderables.hpp"
#include "sys_spatial.hpp"

namespace lithic3d
{

const size_t DYNAMIC_TEXT_MAX_LEN = 31;
const size_t POLYGON_MAX_VERTICES = 8;

struct CRender2d
{
  Vec4f colour;
  uint32_t zIndex = 0;
  bool visible = true;
  ScissorId scissor = 0;

  static constexpr ComponentType TypeId = ComponentTypeId::CRender2dTypeId;
};

struct CMesh2d
{
  ResourceId id = NULL_RESOURCE_ID;
  render::MeshFeatureSet features;

  static constexpr ComponentType TypeId = ComponentTypeId::CMesh2dTypeId;
};

struct CMaterial2d
{
  ResourceId id = NULL_RESOURCE_ID;
  render::MaterialFeatureSet features;

  static constexpr ComponentType TypeId = ComponentTypeId::CMaterial2dTypeId;
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

struct CQuad
{
  float radius = 0.f; // For rounded corners

  static constexpr ComponentType TypeId = ComponentTypeId::CQuadTypeId;
};

struct DSprite
{
  using RequiredComponents = type_list<
    CGlobalTransform, CSpatialFlags, CRender2d, CSprite, CMaterial2d
  >;

  ScissorId scissor = 0;
  render::MaterialHandle material;
  Rectf textureRect;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct DQuad
{
  using RequiredComponents = type_list<CGlobalTransform, CSpatialFlags, CRender2d, CQuad>;

  ScissorId scissor = 0;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
  float radius = 0.f;
};

struct DText
{
  using RequiredComponents = type_list<
    CGlobalTransform, CSpatialFlags, CRender2d, CSprite, CMesh2d, CMaterial2d
  >;

  ScissorId scissor = 0;
  render::MaterialHandle material;
  Rectf textureRect;
  std::string text;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

struct DDynamicText
{
  using RequiredComponents = type_list<
    CGlobalTransform, CSpatialFlags, CRender2d, CSprite, CMesh2d, CMaterial2d, CDynamicText
  >;

  ScissorId scissor = 0;
  render::MaterialHandle material;
  Rectf textureRect;
  std::string text;
  size_t maxLength = DYNAMIC_TEXT_MAX_LEN;
  uint32_t zIndex = 0;
  Vec4f colour{ 1.f, 1.f, 1.f, 1.f };
};

class Camera2d;
namespace render { class Renderer; }

class SysRender2d : public System
{
  public:
    virtual double frameRate() const = 0;

    virtual Camera2d& camera() = 0;
    virtual const Camera2d& camera() const = 0;

    virtual render::Renderer& renderer() = 0;

    virtual void addScissor(ScissorId id, const Recti& scissor) = 0;

    virtual void addEntity(EntityId entityId, const DSprite& data) = 0;
    virtual void addEntity(EntityId entityId, const DText& data) = 0;
    virtual void addEntity(EntityId entityId, const DDynamicText& data) = 0;
    virtual void addEntity(EntityId entityId, const DQuad& data) = 0;

    virtual void setZIndex(EntityId entityId, uint32_t zIndex) = 0;
    virtual void setTextureRect(EntityId entityId, const Rectf& textureRect) = 0;
    virtual void setVisible(EntityId entityId, bool visible) = 0;
    virtual void setColour(EntityId entityId, const Vec4f& colour) = 0;
    virtual void updateDynamicText(EntityId entityId, const std::string& text) = 0;

    virtual ~SysRender2d() = default;

    static const SystemId id = RENDER_2D_SYSTEM;
};

using SysRender2dPtr = std::unique_ptr<SysRender2d>;

class RenderResourceLoader;
class Logger;

SysRender2dPtr createSysRender2d(ComponentStore& componentStore, render::Renderer& renderer,
  RenderResourceLoader& renderResourceLoader, Logger& logger);

} // namespace lithic3d
