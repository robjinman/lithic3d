#pragma once

#include "math.hpp"
#include "ecs.hpp"
#include "renderables.hpp"
#include "systems.hpp"
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
  std::vector<float_t> timestamps;
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
  render::MeshHandle mesh;
  render::MaterialHandle material;
  SkinPtr skin;

  // TODO: Make these private?
  bool jointTransformsDirty = false;
  std::vector<Mat4x4f> jointTransforms;
};

// TODO: List required components
struct DModel
{
  bool isInstanced = false;
  RenderItemId animations = NULL_ID;
  std::vector<Submodel> submodels;
};

using DModelPtr = std::unique_ptr<DModel>;

// TODO: List required components
struct DSkybox
{
  Submodel model;
};

using DSkyboxPtr = std::unique_ptr<DSkybox>;

// TODO: List required components
struct DLight
{
  std::vector<Submodel> submodels;
  Vec3f colour;
  float_t ambient = 0.f;
  float_t specular = 0.f;
  float_t zFar = 1500.f; // TODO
};

using DLightPtr = std::unique_ptr<DLight>;

// TODO: List required components
struct DParticleEmitter
{
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
    virtual RenderItemId addAnimations(AnimationSetPtr animations) = 0;
    virtual void removeAnimations(RenderItemId id) = 0;
    virtual void playAnimation(EntityId entityId, const std::string& name) = 0;

    virtual ~SysRender3d() {}

    static const SystemId id = RENDER_3D_SYSTEM;
};

using SysRender3dPtr = std::unique_ptr<SysRender3d>;

class Logger;

SysRender3dPtr createSysRender3d(const Ecs& ecs, render::Renderer& renderer, Logger& logger);

} // namespace lithic3d
