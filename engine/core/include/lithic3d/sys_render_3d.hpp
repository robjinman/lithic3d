#pragma once

#include "renderables.hpp"
#include "sys_spatial.hpp"
#include <set>
#include <map>

namespace lithic3d
{

struct Skin
{
  std::vector<size_t> joints; // Indices into skeleton joints
  std::vector<Mat4x4f> inverseBindMatrices;
};

using SkinPtr = std::unique_ptr<Skin>;

struct AnimationChannel
{
  size_t jointIndex;  // Index into skin
  std::vector<float> timestamps;
  std::vector<Transform> transforms;
};

struct Animation
{
  std::string name;
  // TODO: Duration
  std::vector<AnimationChannel> channels;
};

using AnimationPtr = std::unique_ptr<Animation>;

struct Joint
{
  Transform transform;
  std::vector<size_t> children;
};

struct Skeleton
{
  size_t rootNodeIndex = 0;
  std::vector<Joint> joints;
};

using SkeletonPtr = std::unique_ptr<Skeleton>;

struct AnimationSet
{
  SkeletonPtr skeleton;
  std::map<std::string, AnimationPtr> animations;
};

using AnimationSetPtr = std::unique_ptr<AnimationSet>;

struct Submodel
{
  ResourceId mesh;
  render::MeshFeatureSet meshFeatures;
  ResourceId material;
  render::MaterialFeatureSet materialFeatures;
  SkinPtr skin;

  // TODO: Make these private?
  bool jointTransformsDirty = false;
  std::vector<Mat4x4f> jointTransforms;
};

struct DModel
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  DModel() {}

  DModel(const DModel& cpy)
  {
    isInstanced = cpy.isInstanced;
    animations = cpy.animations;
    for (auto& m : cpy.submodels) {
      submodels.push_back(Submodel{
        .mesh = m.mesh,
        .material = m.material,
        .skin = m.skin == nullptr ? nullptr : std::make_unique<Skin>(*m.skin),
        .jointTransformsDirty = false,
        .jointTransforms{}
      });
    }
  }

  bool isInstanced = false;
  ResourceId animations = NULL_RESOURCE_ID;
  std::vector<Submodel> submodels;
};

using DModelPtr = std::unique_ptr<DModel>;

struct DSkybox
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  Submodel model;
};

using DSkyboxPtr = std::unique_ptr<DSkybox>;

struct DLight
{
  using RequiredComponents = type_list<CSpatialFlags, CGlobalTransform>;

  std::vector<Submodel> submodels;
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

    // Animations
    //
    virtual ResourceId addAnimations(AnimationSetPtr animations) = 0;
    virtual void removeAnimations(ResourceId id) = 0;
    virtual void playAnimation(EntityId entityId, const std::string& name) = 0;

    virtual ~SysRender3d() {}

    static const SystemId id = RENDER_3D_SYSTEM;
};

using SysRender3dPtr = std::unique_ptr<SysRender3d>;

class Logger;

SysRender3dPtr createSysRender3d(const Ecs& ecs, render::Renderer& renderer, Logger& logger);

} // namespace lithic3d
