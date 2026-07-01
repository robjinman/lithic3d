#pragma once

#include "renderables.hpp"
#include "sys_spatial.hpp"
#include "model_loader.hpp"
#include <set>
#include <map>

namespace lithic3d
{

struct LodMeshData
{
  ResourceId id = NULL_RESOURCE_ID;
  render::MeshFeatureSet features;
};

struct SubmodelData
{
  std::array<LodMeshData, 3> lods;
  uint32_t numLods = 0;
  render::MaterialHandle material;
  bool hasSkin = false;
  //SkinPtr skin;
  //bool jointTransformsDirty = false;
  //std::vector<Mat4x4f> jointTransforms;
};

struct CModel
{
  ResourceHandle model;
  bool isInstanced = false;
  Vec4f colour = { 1.f, 1.f, 1.f, 1.f };
  std::array<SubmodelData, 12> submodels;
  uint32_t numSubmodels = 0;
  //render::RenderItemKey renderItemKey = 0;

  static constexpr ComponentTypeId TypeId = CModelTypeId;
};

struct DModel
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform, CModel>;

  ResourceHandle model;
  bool isInstanced = false;
  Vec4f colour = { 1.f, 1.f, 1.f, 1.f };
};

using DModelPtr = std::unique_ptr<DModel>;

struct DSkybox
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  SubmodelPtr model;
};

using DSkyboxPtr = std::unique_ptr<DSkybox>;

struct DPointLight
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  std::vector<SubmodelPtr> submodels;
  Vec3f colour;
  float ambient = 0.f;
  float specular = 0.f;
};

using DPointLightPtr = std::unique_ptr<DPointLight>;

struct DDirectionalLight
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  Vec3f colour;
  float ambient = 0.f;
  float specular = 0.f;
};

using DDirectionalLightPtr = std::unique_ptr<DDirectionalLight>;

struct DParticleEmitter
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  render::MaterialHandle material;
  Vec4f colour = { 1.f, 0.f, 0.f, 1.f };
  uint32_t numParticles = 100;
  float sizeMean = 2.f;
  float sizeVariance = 1.f;
};

using DParticleEmitterPtr = std::unique_ptr<DParticleEmitter>;

class Camera3d;
namespace render { class Renderer; }

class SysRender3d : public System
{
  public:
    using System::addEntity;  // Silence clang warning -Woverloaded-virtual

    virtual double frameRate() const = 0;

    virtual Camera3d& camera() = 0;
    virtual const Camera3d& camera() const = 0;

    virtual void preupdate() = 0;

    virtual render::Renderer& renderer() = 0;

    // TODO: Const references instead of pointers?
    virtual void addEntity(EntityId id, DModelPtr model) = 0;
    virtual void addEntity(EntityId id, DPointLightPtr light) = 0;
    virtual void addEntity(EntityId id, DDirectionalLightPtr light) = 0;
    virtual void addEntity(EntityId id, DSkyboxPtr skybox) = 0;
    virtual void addEntity(EntityId id, DParticleEmitterPtr particleEmitter) = 0;

    virtual void setEntityColour(EntityId id, const Vec4f& colour) = 0;

    // TODO: Implement scissors?

    virtual void playAnimation(EntityId entityId, const std::string& name, bool repeat = false) = 0;

    virtual ~SysRender3d() {}

    static const SystemId id = Systems::Render3d;
};

using SysRender3dPtr = std::unique_ptr<SysRender3d>;

// ----- Exposed for testing
struct LightProjection
{
  Vec3f pos;
  Mat4x4f viewMatrix;
  Mat4x4f projectionMatrix;
  Frustum frustum;
};

LightProjection computeLightProjection(const std::array<Vec3f, 8>& corners,
  const Vec3f& worldSpaceLightDir);
// -----

class Logger;
class EventSystem;

SysRender3dPtr createSysRender3d(float drawDistance, Ecs& ecs, ModelLoader& modelLoader,
  render::Renderer& renderer, EventSystem& eventSystem, Logger& logger);

} // namespace lithic3d
