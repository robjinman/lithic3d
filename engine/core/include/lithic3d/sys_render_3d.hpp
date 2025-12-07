#pragma once

#include "renderables.hpp"
#include "sys_spatial.hpp"
#include "model_loader.hpp"
#include <set>
#include <map>

namespace lithic3d
{

struct DModel
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  ResourceHandle model;
  bool isInstanced = false;
};

using DModelPtr = std::unique_ptr<DModel>;

struct DSkybox
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  SubmodelPtr model;
};

using DSkyboxPtr = std::unique_ptr<DSkybox>;

struct DLight
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  std::vector<SubmodelPtr> submodels;
  Vec3f colour;
  float ambient = 0.f;
  float specular = 0.f;
  float zFar = 1500.f; // TODO
};

using DLightPtr = std::unique_ptr<DLight>;

struct DParticleEmitter
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  // TODO
};

using DParticleEmitterPtr = std::unique_ptr<DParticleEmitter>;

class Camera3d;
namespace render { class Renderer; }

class SysRender3d : public System
{
  public:
    virtual double frameRate() const = 0;

    virtual Camera3d& camera() = 0;
    virtual const Camera3d& camera() const = 0;

    virtual render::Renderer& renderer() = 0;

    virtual void addEntity(EntityId id, DModelPtr model) = 0;
    virtual void addEntity(EntityId id, DLightPtr light) = 0;
    virtual void addEntity(EntityId id, DSkyboxPtr skybox) = 0;

    // TODO: Implement scissors?

    virtual void playAnimation(EntityId entityId, const std::string& name) = 0;

    virtual ~SysRender3d() {}

    static const SystemId id = RENDER_3D_SYSTEM;
};

using SysRender3dPtr = std::unique_ptr<SysRender3d>;

class Logger;

SysRender3dPtr createSysRender3d(const Ecs& ecs, const ModelLoader& modelLoader,
  render::Renderer& renderer, Logger& logger);

} // namespace lithic3d
